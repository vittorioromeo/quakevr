/*======================================================================
 Particle EMITTER functions
======================================================================*/
float(entity pe_ent) misc_particle_style =
{
    pe_ent.start_delay = 1 + (random()*2);    // Particle emitter start delay (default)
    pe_ent.spr_frame = 1;                    // Use all sprite frames (Light/Dark)
    pe_ent.dpp_name = "";                    // DP particle effect name
    pe_ent.dpp_wait = 0.1;                    // Defautlt timer
    pe_ent.dpp_rnd = 1;                        // Random time multiplier
    pe_ent.dpp_vel = '0 0 0';                // Velocity direction of particles
    pe_ent.part_movetype = MOVETYPE_NOCLIP;

    //----------------------------------------------------------------------
    // Portal Gate
    //----------------------------------------------------------------------
    if (pe_ent.part_style == PARTICLE_STYLE_PORTAL) {
        pe_ent.spr_name1 = PART_DOTMED_GREY;
        pe_ent.spr_name2 = PART_DOTSML_WHITE;
        pe_ent.spr_name3 = PART_DOTSML_WHITE;
        pe_ent.spr_frame = 3;                    // Dark frames
        pe_ent.part_limit = 100;                // High Amount of particles
        pe_ent.part_life = 3;                    // Long life time
        pe_ent.part_ofs = '0 0 0';                // No change
        pe_ent.part_veltype = PARTICLE_ORIGIN_VOL;

        // If the angle is up or down then it is a special case
        if (pe_ent.angles_y == -1) {
            pe_ent.part_vel = '0 0 20';            // UP direction
            pe_ent.dpp_name = DPP_PORTALUP;        // particles flying up
        }
        else if (pe_ent.angles_y == -2) {
            pe_ent.part_vel = '0 0 -20';        // DOWN direction
            pe_ent.dpp_name = DPP_PORTALDOWN;    // smoke falling down
        }
        else {
            // Setup DP portal effects volume size based on initial emitter angle
            if (pe_ent.angles_y == 90 || pe_ent.angles_y == 270) {
                if (!pe_ent.part_vol) pe_ent.part_vol = '32 0 48';    // Volume (X/Y/Z)
                pe_ent.dpp_name = DPP_PORTALSIDE;
            }
            else {
                if (!pe_ent.part_vol) pe_ent.part_vol = '0 32 48';    // Volume (X/Y/Z)
                pe_ent.dpp_name = DPP_PORTALFRONT;        // default direction
            }

            // The angle of the particle entity is using the ANGLE key
            makevectors(pe_ent.angles);
            pe_ent.part_vel = v_forward * 20;            // Slow speed away from portal
        }

        if (!pe_ent.part_velrand) pe_ent.part_velrand = '10 10 4';    // More X/Y wobble
        if (!pe_ent.wakeup_dist) pe_ent.wakeup_dist = 768;    // Mid range distance
        pe_ent.wakeup_timer = 4;                            // High timer (runs all the time)
        if (!pe_ent.spawn_base) pe_ent.spawn_base = 0.1;    // Fast Spawn rate
        if (!pe_ent.spawn_rand) pe_ent.spawn_rand = 0.1;    // Spawn rate randomness
        pe_ent.dpp_vel = normalize(pe_ent.part_vel);        // Particle direction
    }
    //----------------------------------------------------------------------
    // Spiral Jumppad pattern
    //----------------------------------------------------------------------
    else if (pe_ent.part_style == PARTICLE_STYLE_JUMPPAD) {
        pe_ent.spr_name1 = PART_DOTMED_GREY;
        pe_ent.spr_name2 = PART_DOTSML_WHITE;
        pe_ent.spr_name3 = PART_DOTSML_GOLD;
        pe_ent.part_limit = 100;            // High Amount of particles
        pe_ent.part_life = 3;                // Long life time
        pe_ent.part_ofs = '0 0 0';            // Start below emitter origin
        pe_ent.part_velbase = '0 0 12';        // Always go upwards
        pe_ent.part_vel = '0 0 32';            // UP direction
        pe_ent.part_velrand = '8 8 0';        // Slight wobble
        pe_ent.part_vol = '24 24 0';        // Volume around circle pad
        pe_ent.part_veltype = PARTICLE_ORIGIN_VOL;    // inside circle volume
        pe_ent.wakeup_dist = 1024;            // Long range distance
        pe_ent.wakeup_timer = 2;            // High timer (runs all the time)
        pe_ent.spawn_base = 0.05;            // Average Spawn rate
        pe_ent.spawn_rand = 0.05;            // Spawn rate randomness
        pe_ent.dpp_name = DPP_JUMPPAD;        // streaming up
    }
    //----------------------------------------------------------------------
    // Circular floor pattern (triggered)
    //----------------------------------------------------------------------
    else if (pe_ent.part_style == PARTICLE_STYLE_FCIRCLE) {
        pe_ent.spr_name1 = PART_TORCH1;
        pe_ent.spr_name2 = PART_TORCH2;
        pe_ent.spr_name3 = PART_DOTSML_GOLD;
        pe_ent.part_limit = 64;                // High Amount of particles
        pe_ent.part_life = 5;                // Long life time
        pe_ent.part_ofs = '0 0 1';            // Slightly off floor
        pe_ent.part_vel = '0 0 12';            // Slowly move upwards
        pe_ent.part_velrand = '4 4 0';        // Slight wobble
        // Circle radius/rotation degrees
        if (CheckZeroVector(pe_ent.pemit_source.part_vol)) pe_ent.part_vol = '56 17 0';
        pe_ent.part_veltype = PARTICLE_ORIGIN_RANDCIRCLE;
        pe_ent.wakeup_dist = 768;            // Mid range distance
        pe_ent.wakeup_timer = 0.5;            // 4x timer
        // Fast spawn rate and randomness
        if (!pe_ent.pemit_source.spawn_base) pe_ent.spawn_base = 0.1;            // Fast Spawn rate
        if (!pe_ent.pemit_source.spawn_rand) pe_ent.spawn_rand = 0.2;            // Spawn rate randomness
        pe_ent.dpp_name = DPP_FCIRCLE;        // Circular floor pattern
        pe_ent.dpp_rnd = 0.2;                // Random time multiplier
    }
    //----------------------------------------------------------------------
    // Forcefield
    //----------------------------------------------------------------------
    else if (pe_ent.part_style == PARTICLE_STYLE_FFIELD) {
        if (pe_ent.pemit_source.spr_frame == PARTICLE_BURST_YELLOW) {
            pe_ent.spr_name1 = PART_DOTSML_GOLD;
            pe_ent.spr_name2 = PART_DOTSML_YELLOW;
            pe_ent.spr_name3 = PART_DOTMED_YELLOW;
            pe_ent.dpp_name = DPP_FFIELDPARTY;
        }
        else if (pe_ent.pemit_source.spr_frame == PARTICLE_BURST_GREEN) {
            pe_ent.spr_name1 = PART_DOTSML_LGREEN;
            pe_ent.spr_name2 = PART_DOTSML_GREEN;
            pe_ent.spr_name3 = PART_DOTMED_GREEN;
            pe_ent.dpp_name = DPP_FFIELDPARTG;
        }
        else if (pe_ent.pemit_source.spr_frame == PARTICLE_BURST_RED) {
            pe_ent.spr_name1 = PART_DOTSML_GREY;
            pe_ent.spr_name2 = PART_DOTSML_RED;
            pe_ent.spr_name3 = PART_DOTMED_RED;
            pe_ent.dpp_name = DPP_FFIELDPARTR;
        }
        else if (pe_ent.pemit_source.spr_frame == PARTICLE_BURST_BLUE) {
            pe_ent.spr_name1 = PART_DOTSML_BLUE;
            pe_ent.spr_name2 = PART_DOTSML_GREY;
            pe_ent.spr_name3 = PART_DOTMED_BLUE;
            pe_ent.dpp_name = DPP_FFIELDPARTB;
        }
        else if (pe_ent.pemit_source.spr_frame == PARTICLE_BURST_PURPLE) {
            pe_ent.spr_name1 = PART_DOTSML_PURP;
            pe_ent.spr_name2 = PART_DOTSML_GREY;
            pe_ent.spr_name3 = PART_DOTMED_PURP;
            pe_ent.dpp_name = DPP_FFIELDPARTP;
        }
        else if (pe_ent.pemit_source.spr_frame == PARTICLE_BURST_FIRE) {
            pe_ent.spr_name1 = PART_TORCH1;
            pe_ent.spr_name2 = PART_DOTSML_GOLD;
            pe_ent.spr_name3 = PART_DOTMED_GREY;
            pe_ent.dpp_name = DPP_FFIELDPARTY;
        }
        else {
            pe_ent.spr_name1 = PART_DOTSML_WHITE;
            pe_ent.spr_name2 = PART_DOTSML_GREY;
            pe_ent.spr_name3 = PART_DOTMED_GREY;
            pe_ent.dpp_name = DPP_FFIELDPARTW;
        }

        // reset particle sprite frame range to all
        pe_ent.spr_frame = 0;
        
        // Particle quantity should be low, allow customization for volume size
        if (!pe_ent.pemit_source.part_limit) pe_ent.part_limit = 25;
        else pe_ent.part_limit = pe_ent.pemit_source.part_limit;
        if (!pe_ent.pemit_source.part_life) pe_ent.part_life = 2;
        else pe_ent.part_life = pe_ent.pemit_source.part_life;
        if (!pe_ent.pemit_source.part_ofs) pe_ent.part_ofs = '0 0 0';
        else pe_ent.part_ofs = pe_ent.pemit_source.part_ofs;
        pe_ent.part_veltype = PARTICLE_ORIGIN_VOL;
        if (!pe_ent.part_velrand) pe_ent.part_velrand = '4 4 4';
        // The volume and direction really need to be set before the
        // emitter is setup because the volume maybe switched off
        if (CheckZeroVector(pe_ent.pemit_source.part_vol)) pe_ent.part_vol = '8 8 8';
        else pe_ent.part_vol = pe_ent.pemit_source.part_vol;
        if (CheckZeroVector(pe_ent.pemit_source.part_vel)) pe_ent.part_vel = '0 0 4';
        else pe_ent.part_vel = pe_ent.pemit_source.part_vel;

        if (!pe_ent.pemit_source.wakeup_dist) pe_ent.wakeup_dist = 768;    // Mid range distance
        pe_ent.wakeup_timer = 4;                            // High timer (runs all the time)
        if (!pe_ent.pemit_source.spawn_base) pe_ent.spawn_base = 0.05;    // Fast Spawn rate
        if (pe_ent.spawn_base < 0.01) pe_ent.spawn_base = 0.01;            // Need minimum
        if (!pe_ent.pemit_source.spawn_rand) pe_ent.spawn_rand = 0.1;    // Spawn rate randomness
        pe_ent.dpp_vel = pe_ent.part_vel;
        pe_ent.dpp_wait = 0.01;
        pe_ent.dpp_rnd = 0.1;
    }

    //======================================================================
    // Book Phase 1 - closed
    //----------------------------------------------------------------------
    else if (pe_ent.part_style == PARTICLE_STYLE_BOOK) {
        pe_ent.spr_name1 = PART_BOOKRUNE1;
        pe_ent.spr_name2 = PART_BOOKRUNE2;
        if (random()<0.5) pe_ent.spr_name3 = PART_BOOKRUNE1;
        else pe_ent.spr_name3 = PART_BOOKRUNE2;
        pe_ent.part_limit = 3;                // Very Low
        pe_ent.part_life = 7;                // Long Life
        pe_ent.part_ofs = '0 0 10';            // Top of Book
        pe_ent.part_vel = '0 0 10';            // Fly up
        pe_ent.part_velrand = '4 4 0';        // Slight wobble
        pe_ent.part_velrot = 45;            // Velocity rotation - y + random(y)
        pe_ent.part_vol = '1 1 0';            // Volume (X/Y/Z)
        pe_ent.part_veltype = PARTICLE_ORIGIN_VOL;    // Inside volume
        pe_ent.wakeup_dist = 768;            // Close range distance
        pe_ent.wakeup_timer = 8;            // High timer
        pe_ent.spawn_base = 2;                // Slow (runs all the time)
        pe_ent.spawn_rand = 1;                // Spawn rate randomness
        if (pe_ent.pemit_source.style == MISCBOOK_BLUE) pe_ent.dpp_name = DPP_BOOKBLUE;
        else if (pe_ent.pemit_source.style == MISCBOOK_RED) pe_ent.dpp_name = DPP_BOOKRED;
        else pe_ent.dpp_name = DPP_BOOKGOLD;
        pe_ent.dpp_wait = 1;                // Slow timer
        pe_ent.dpp_rnd = 2;                    // Long pause
    }
    //----------------------------------------------------------------------
    // Book Phase 2 - open
    //----------------------------------------------------------------------
    else if (pe_ent.part_style == PARTICLE_STYLE_OPENBOOK) {
        if (random()<0.5) pe_ent.spr_name1 = PART_BOOKRUNE1;
        else pe_ent.spr_name1 = PART_BOOKRUNE2;

        // Setup particle colour based on book type
        if (pe_ent.pemit_source.style == MISCBOOK_BLUE) {
            if (random() < 0.5) pe_ent.spr_name2 = PART_DOTMED_BLUE;
            else pe_ent.spr_name2 = PART_DOTSML_BLUE;
            pe_ent.spr_name3 = PART_DOTSML_WHITE;
            pe_ent.dpp_name = DPP_OPENBOOKBLUE;
        }
        else if (pe_ent.pemit_source.style == MISCBOOK_RED) {
            if (random() < 0.5) pe_ent.spr_name2 = PART_DOTMED_RED;
            else pe_ent.spr_name2 = PART_DOTSML_RED;
            pe_ent.spr_name3 = PART_DOTSML_GOLD;
            pe_ent.dpp_name = DPP_OPENBOOKRED;
        }
        else {
            if (random() < 0.5) pe_ent.spr_name2 = PART_DOTMED_YELLOW;
            else pe_ent.spr_name2 = PART_DOTSML_YELLOW;
            pe_ent.spr_name3 = PART_DOTSML_GOLD;
            pe_ent.dpp_name = DPP_OPENBOOKGOLD;
        }
        pe_ent.part_limit = 30;                // Average
        pe_ent.part_life = 7;                // Long Life
        pe_ent.part_ofs = '0 0 4';            // Pages of Book
        pe_ent.part_vel = '0 0 10';            // Fly up
        pe_ent.part_velrand = '4 4 0';        // Slight wobble
        pe_ent.part_velrot = 45;            // Velocity rotation - y + random(y)
        pe_ent.part_vol = '8 8 0';            // Volume (X/Y/Z)
        pe_ent.part_veltype = PARTICLE_ORIGIN_VOL;    // Inside volume
        pe_ent.wakeup_dist = 768;            // Close range distance
        pe_ent.wakeup_timer = 4;            // High timer
        pe_ent.spawn_base = 0.1;            // Fast (particles)
        pe_ent.spawn_rand = 0.3;            // Spawn rate randomness
        pe_ent.dpp_wait = 0.5;                // Fast timer
        pe_ent.dpp_rnd = 2;                    // Long pause
    }
    //======================================================================
    // Electric Beam Particle Stream (code triggered)
    //----------------------------------------------------------------------
    else if (pe_ent.part_style == PARTICLE_STYLE_ELECTRIC) {
        pe_ent.spr_name1 = PART_DOTMED_BLUE;
        pe_ent.spr_name2 = PART_DOTSML_GOLD;
        pe_ent.spr_name3 = PART_DOTSML_BLUE;
        pe_ent.part_limit = 100;            // Maximum particles
        pe_ent.part_life = 1;                // Short Lifetime
        pe_ent.part_ofs = '0 0 0';            // center of target object
        pe_ent.part_vel = '50 0 0';            // Speed of particles
        pe_ent.part_velrand = '0 0 16';        // Vertical wobble
        pe_ent.part_vol = '8 8 8';            // Volume (X/Y/Z)
        pe_ent.part_veltype = PARTICLE_ORIGIN_VOL;    // Inside volume
        pe_ent.wakeup_dist = 768;            // Long range distance
        pe_ent.wakeup_timer = 0.2;            // Small change on distance timer
        pe_ent.spawn_base = 0.01;            // Fast Spawn rate
        pe_ent.spawn_rand = 0;                // Spawn rate randomness
        pe_ent.dpp_name = DPP_ELECTRIC;        // directional particle stream
        pe_ent.dpp_rnd = 0;                    // NO Random time multiplier
    }
    //----------------------------------------------------------------------
    // Torches Flames (slow start)
    //----------------------------------------------------------------------
    else if (pe_ent.part_style == PARTICLE_STYLE_FLAMET) {
        pe_ent.start_delay = 3 + (random()*8);
        pe_ent.spr_name1 = PART_TORCH1;        // Embers - Red/Yellow
        pe_ent.spr_name2 = PART_DOTSML_GOLD;
        pe_ent.spr_name3 = PART_DOTSML_GREY;
        pe_ent.part_limit = 8;                // Minimal amount of particles
        pe_ent.part_life = 2;                // Short Life
        pe_ent.part_ofs = '0 0 12';            // Top of wood
        pe_ent.part_vel = '0 0 10';            // Fly up
        pe_ent.part_velrand = '4 4 0';        // Slight wobble
        pe_ent.part_vol = '1 1 0';            // Volume (X/Y/Z)
        pe_ent.part_veltype = PARTICLE_ORIGIN_VOL;    // Inside volume
        if (pe_ent.pemit_source.wakeup_dist < 1) pe_ent.wakeup_dist = 384;
        else pe_ent.wakeup_dist = pe_ent.pemit_source.wakeup_dist;
        if (pe_ent.pemit_source.wakeup_timer < 1) pe_ent.wakeup_timer = 8;
        else pe_ent.wakeup_timer = pe_ent.pemit_source.wakeup_timer;
        pe_ent.spawn_base = 0.25;            // Spawn rate base (runs all the time)
        pe_ent.spawn_rand = 0.25;            // Spawn rate randomness
        pe_ent.dpp_name = DPP_FLAMET;        // smoke and particles floating up
        pe_ent.dpp_wait = 1;                // Slow timer
    }
    //----------------------------------------------------------------------
    // Small Flames (slow start)
    //----------------------------------------------------------------------
    else if (pe_ent.part_style == PARTICLE_STYLE_FLAMES) {
        pe_ent.start_delay = 3 + (random()*8);
        pe_ent.spr_name1 = PART_TORCH1;        // Embers - Red/Yellow
        pe_ent.spr_name2 = PART_DOTSML_GOLD;
        pe_ent.spr_name3 = PART_DOTSML_GREY;
        pe_ent.part_limit = 8;                // Minimal amount of particles
        pe_ent.part_life = 2;                // Short Life
        pe_ent.part_ofs = '0 0 0';            // Bottom of flame
        pe_ent.part_vel = '0 0 10';            // Fly up
        pe_ent.part_velrand = '4 4 0';        // Slight wobble
        pe_ent.part_vol = '1 1 0';            // Volume (X/Y/Z)
        pe_ent.part_veltype = PARTICLE_ORIGIN_VOL;    // Inside volume
        if (pe_ent.pemit_source.wakeup_dist < 1) pe_ent.wakeup_dist = 384;
        else pe_ent.wakeup_dist = pe_ent.pemit_source.wakeup_dist;
        if (pe_ent.pemit_source.wakeup_timer < 1) pe_ent.wakeup_timer = 6;
        else pe_ent.wakeup_timer = pe_ent.pemit_source.wakeup_timer;
        pe_ent.spawn_base = 0.25;            // Spawn rate base (runs all the time)
        pe_ent.spawn_rand = 0.25;            // Spawn rate randomness
        pe_ent.dpp_name = DPP_FLAMES;        // smoke and particles floating up
        pe_ent.dpp_wait = 1;                // Slow timer
    }
    //----------------------------------------------------------------------
    // Large Flames (high particle count)
    //----------------------------------------------------------------------
    else if (pe_ent.part_style == PARTICLE_STYLE_FLAMEL) {
        pe_ent.spr_name1 = PART_TORCH1;
        pe_ent.spr_name2 = PART_DOTSML_GOLD;
        pe_ent.spr_name3 = PART_DOTSML_GREY;
        pe_ent.part_limit = 100;            // Large amount of active particles
        if (pe_ent.pemit_source.part_life < 1) pe_ent.part_life = 8;
        else pe_ent.part_life = pe_ent.pemit_source.part_life;
        pe_ent.part_ofs = '0 0 0';            // Bottom of flame
        pe_ent.part_vel = '0 0 12';            // Fly up
        pe_ent.part_velrand = '4 4 0';        // Slight wobble
        pe_ent.part_vol = '4 4 0';            // Volume (X/Y/Z)
        pe_ent.part_veltype = PARTICLE_ORIGIN_VOL;    // Inside volume
        if (pe_ent.pemit_source.wakeup_dist < 1) pe_ent.wakeup_dist = 768;
        else pe_ent.wakeup_dist = pe_ent.pemit_source.wakeup_dist;
        if (pe_ent.pemit_source.wakeup_timer < 1) pe_ent.wakeup_timer = 1;
        else pe_ent.wakeup_timer = pe_ent.pemit_source.wakeup_timer;
        pe_ent.spawn_base = 0.05;            // Frequent (runs all the time)
        pe_ent.spawn_rand = 0.1;            // Spawn rate randomness
        pe_ent.dpp_name = DPP_FLAMEL;        // smoke and particles floating up
    }
    //----------------------------------------------------------------------
    // Skill Particle Stream (triggered)
    //----------------------------------------------------------------------
    else if (pe_ent.part_style == PARTICLE_STYLE_SKILL) {
        pe_ent.spr_name1 = PART_DOTMED_RED;
        pe_ent.spr_name2 = PART_DOTSML_GOLD;
        pe_ent.spr_name3 = PART_DOTSML_RED;
        pe_ent.part_limit = 35;                // Maximum particles
        pe_ent.part_life = 2;                // Long life
        pe_ent.part_ofs = '0 0 0';            // center of target object
        // The angle of the particle entity is using the ANGLE key
        makevectors(pe_ent.pemit_source.angles);
        pe_ent.part_vel = v_forward * 10;    // Slow speed away from skill column
        pe_ent.part_vol = '4 4 4';            // Volume (X/Y/Z)
        pe_ent.part_velrand = '4 4 8';        // Volume wobble
        pe_ent.part_veltype = PARTICLE_ORIGIN_VOL;    // Inside volume
        pe_ent.wakeup_dist = 768;            // Long range distance
        pe_ent.wakeup_timer = 1;            // Small change on distance timer
        pe_ent.spawn_base = 0.01;            // Fast Spawn rate
        pe_ent.spawn_rand = 0.2;            // Spawn rate randomness
        pe_ent.dpp_name = DPP_SKILLPILLAR;    // Directional particle stream
        pe_ent.dpp_rnd = 0;                    // NO Random time multiplier
        pe_ent.dpp_vel = normalize(v_forward);    // Particle direction
    }
    //----------------------------------------------------------------------
    // Particles raising up around an altar
    //----------------------------------------------------------------------
    else if (pe_ent.part_style == PARTICLE_STYLE_ALTAR) {
        if (pe_ent.pemit_source.cnt == 1) {
            pe_ent.spr_name1 = PART_DOTMED_GREY;
            pe_ent.spr_name2 = PART_DOTSML_WHITE;
            pe_ent.spr_name3 = PART_DOTSML_GOLD;
            pe_ent.dpp_name = DPP_ALTARGREY;    // floating upwards
        }
        else {
            pe_ent.spr_name1 = PART_DOTMED_RED;
            pe_ent.spr_name2 = PART_DOTSML_GOLD;
            pe_ent.spr_name3 = PART_DOTSML_RED;
            pe_ent.dpp_name = DPP_ALTARRED;        // floating upwards
        }
        pe_ent.part_limit = 100;            // Maximum particles
        pe_ent.part_life = 5;                // Long life
        pe_ent.part_ofs = '0 0 0';            // center of target object
        if (CheckZeroVector(pe_ent.pemit_source.part_vel)) pe_ent.part_vel = '0 0 12';
        pe_ent.part_velrand = '4 4 12';        // Slight wobble
        pe_ent.part_vol = '64 64 0';        // Volume (X/Y/Z)
        if (pe_ent.pemit_source.t_length > 0) 
            pe_ent.part_vol_x = pe_ent.pemit_source.t_length;
        if (pe_ent.pemit_source.t_width > 0) 
            pe_ent.part_vol_y = pe_ent.pemit_source.t_width;
        pe_ent.part_veltype = PARTICLE_ORIGIN_VOL;    // Inside volume
        pe_ent.wakeup_dist = 768;            // Close range distance
        pe_ent.wakeup_timer = 4;            // High timer
        pe_ent.spawn_base = 0.1;            // Fast Spawn rate
        pe_ent.spawn_rand = 0.1;            // Spawn rate randomness
    }
    //----------------------------------------------------------------------
    // DP ONLY - velocity directional smoke
    //----------------------------------------------------------------------
    else if (pe_ent.part_style == PARTICLE_STYLE_SMOKE) {
        pe_ent.dpp_only = TRUE;
        if (pe_ent.pemit_source.exactskin == 0) pe_ent.dpp_name = DPP_VELSMOKEGREY1;
        else if (pe_ent.pemit_source.exactskin == 1) pe_ent.dpp_name = DPP_VELSMOKEGREY2;
        else if (pe_ent.pemit_source.exactskin == 2) pe_ent.dpp_name = DPP_VELSMOKEWHITE;
        else if (pe_ent.pemit_source.exactskin == 3) pe_ent.dpp_name = DPP_VELSMOKETOXIC;
        else if (pe_ent.pemit_source.exactskin == 4) pe_ent.dpp_name = DPP_VELSMOKEGREEN;
        else if (pe_ent.pemit_source.exactskin == 5) pe_ent.dpp_name = DPP_VELSMOKEPURPLE;
        else if (pe_ent.pemit_source.exactskin == 6) pe_ent.dpp_name = DPP_VELSMOKERED;
        else if (pe_ent.pemit_source.exactskin == 7) pe_ent.dpp_name = DPP_VELSMOKEFIRE;
        pe_ent.dpp_rnd = pe_ent.pemit_source.delay;
        pe_ent.dpp_wait = pe_ent.pemit_source.wait;
        pe_ent.dpp_vel = pe_ent.pemit_source.movedir * pe_ent.pemit_source.height;
    }
    //======================================================================
    // Items
    //======================================================================
    else if (pe_ent.part_style == PARTICLE_STYLE_RESPAWN) {
        pe_ent.start_delay = 0.1;            // Particle emitter start delay
        pe_ent.spr_name1 = PART_DOTSML_YELLOW;
        pe_ent.spr_name2 = PART_DOTSML_GREY;
        pe_ent.spr_name3 = PART_DOTSML_GOLD;
        pe_ent.part_limit = 25;                // Variable, depends on spawn rate
        pe_ent.part_life = 1;                // Short life time
        pe_ent.part_ofs = '0 0 1';            // Custom for each item
        pe_ent.part_vel = '0 0 10';            // Slowly move upwards
        pe_ent.part_vol = '16 16 0';        // Flat base of model
        pe_ent.part_velrand = '4 4 6';        // Slight wobble
        pe_ent.part_veltype = PARTICLE_ORIGIN_VOL;    // Inside volume
        pe_ent.wakeup_dist = 768;            // Mid range distance
        pe_ent.wakeup_timer = 1;            // Medium timer (runs all the time)
        pe_ent.spawn_base = 0.05;            // Fast Spawn rate
        pe_ent.spawn_rand = 0;                // Spawn rate randomness
        pe_ent.dpp_name = DPP_RESPAWN;        // Particles floating up, starts slow
        pe_ent.dpp_rnd = 0;                    // NO Random time multiplier
    }
    //----------------------------------------------------------------------
    // Items - Mega Health
    //----------------------------------------------------------------------
    else if (pe_ent.part_style == PARTICLE_STYLE_MEGAH) {
        pe_ent.spr_name1 = PART_DOTSML_WHITE;
        pe_ent.spr_name2 = PART_DOTMED_RED;
        pe_ent.spr_name3 = PART_DOTSML_GOLD;
        pe_ent.part_limit = 75;                // High Amount of particles
        pe_ent.part_life = 3.5;                // Short life time
        pe_ent.part_ofs = '0 0 16';            // Move up to the top of the key
        pe_ent.part_vel = '12 12 4';        // Speed of particles outwards
        pe_ent.part_velrand = '0 0 8';        // Slight wobble
        pe_ent.part_veltype = PARTICLE_ORIGIN_CENTER;    // Explosion
        pe_ent.wakeup_dist = 768;            // Mid range distance
        pe_ent.wakeup_timer = 1;            // Medium timer (runs all the time)
        pe_ent.spawn_base = 0.05;            // Fast Spawn rate
        pe_ent.spawn_rand = 0.1;            // Spawn rate randomness
        pe_ent.dpp_name = DPP_MEGAH;        // Particles drifting out from center
    }
    //----------------------------------------------------------------------
    // Item - Armor (Green/Yellow/Red)
    //----------------------------------------------------------------------
    else if (pe_ent.part_style == PARTICLE_STYLE_ARMOR) {
        if (pe_ent.pemit_source.items & IT2_ARMOR1) {
            pe_ent.spr_name1 = PART_DOTSML_LGREEN;
            pe_ent.spr_name2 = PART_DOTMED_LGREEN;
            pe_ent.spr_name3 = PART_DOTSML_GREY;
            pe_ent.dpp_name = DPP_ARMOR1;    // Green particles + smoke
        }
        else if (pe_ent.pemit_source.items & IT2_ARMOR2) {
            if (pe_ent.pemit_source.spawnflags & ARMOR_BLUE) {
                pe_ent.spr_name1 = PART_DOTSML_BLUE;
                pe_ent.spr_name2 = PART_DOTMED_BLUE;
                pe_ent.spr_name3 = PART_DOTSML_GREY;
                pe_ent.dpp_name = DPP_ARMOR2BLUE;// Blue particles + smoke
            }
            else {
                pe_ent.spr_name1 = PART_DOTSML_YELLOW;
                pe_ent.spr_name2 = PART_DOTMED_YELLOW;
                pe_ent.spr_name3 = PART_DOTSML_GOLD;
                pe_ent.dpp_name = DPP_ARMOR2;    // Yellow particles + smoke
            }
        }
        else {
            pe_ent.spr_name1 = PART_DOTSML_RED;
            pe_ent.spr_name2 = PART_DOTSML_GOLD;
            pe_ent.spr_name3 = PART_DOTMED_RED;
            pe_ent.dpp_name = DPP_ARMOR3;    // Red particles + smoke
        }
        pe_ent.part_limit = 50;                // High Amount of particles
        pe_ent.part_life = 2.75;            // Short life time
        pe_ent.part_ofs = '0 0 24';            // Move up to middle of Suit
        pe_ent.part_vel = '6 6 4';            // Speed of particles outwards
        pe_ent.part_velbase = '0 0 12';        // Move upwards
        pe_ent.part_veltype = PARTICLE_ORIGIN_CENTER;    // Explosion
        pe_ent.wakeup_dist = 768;            // Mid range distance
        pe_ent.wakeup_timer = 1;            // Medium timer (runs all the time)
        pe_ent.spawn_base = 0.1;            // Fast Spawn rate
        pe_ent.spawn_rand = 0.2;            // Spawn rate randomness
    }
    //----------------------------------------------------------------------
    // Item - Silver Key
    //----------------------------------------------------------------------
    else if (pe_ent.part_style == PARTICLE_STYLE_KEYSILVER) {
        pe_ent.spr_name1 = PART_DOTSML_BLUE;
        pe_ent.spr_name2 = PART_DOTSML_BLUE;
        pe_ent.spr_name3 = PART_DOTSML_GREY;
        pe_ent.part_limit = 50;                // High Amount of particles
        pe_ent.part_life = 2;                // Short life time
        pe_ent.part_ofs = '0 0 20';            // Move up to the top of the key
        pe_ent.part_vel = '16 16 8';        // Speed of particles outwards
        pe_ent.part_veltype = PARTICLE_ORIGIN_CENTER;    // Explosion
        pe_ent.wakeup_dist = 768;            // Mid range distance
        pe_ent.wakeup_timer = 1;            // Medium timer (runs all the time)
        pe_ent.spawn_base = 0.1;            // Fast Spawn rate
        pe_ent.spawn_rand = 0;                // Spawn rate randomness
        pe_ent.dpp_name = DPP_KEYSILVER;    // Blue particles from top of key
    }
    //----------------------------------------------------------------------
    // Item - Gold Key
    //----------------------------------------------------------------------
    else if (pe_ent.part_style == PARTICLE_STYLE_KEYGOLD) {
        pe_ent.spr_name1 = PART_DOTSML_GOLD;
        pe_ent.spr_name2 = PART_DOTSML_GOLD;
        pe_ent.spr_name3 = PART_DOTSML_YELLOW;
        pe_ent.part_limit = 50;                // High Amount of particles
        pe_ent.part_life = 2;                // Short life time
        pe_ent.part_ofs = '0 0 20';            // Move up to the top of the key
        pe_ent.part_vel = '16 16 8';        // Speed of particles outwards
        pe_ent.part_veltype = PARTICLE_ORIGIN_CENTER;    // Explosion
        pe_ent.wakeup_dist = 768;            // Mid range distance
        pe_ent.wakeup_timer = 1;            // Medium timer (runs all the time)
        pe_ent.spawn_base = 0.1;            // Fast Spawn rate
        pe_ent.spawn_rand = 0;                // Spawn rate randomness
        pe_ent.dpp_name = DPP_KEYGOLD;        // Yellow particles from top of key
    }
    //----------------------------------------------------------------------
    // Item - Generic Red Key
    //----------------------------------------------------------------------
    else if (pe_ent.part_style == PARTICLE_STYLE_KEYRED) {
        pe_ent.spr_name1 = PART_DOTSML_RED;
        pe_ent.spr_name2 = PART_DOTMED_RED;
        pe_ent.spr_name3 = PART_DOTSML_GOLD;
        pe_ent.part_limit = 50;                // High Amount of particles
        pe_ent.part_life = 2;                // Short life time
        pe_ent.part_ofs = '0 0 20';            // Move up to the top of the key
        pe_ent.part_vel = '16 16 8';        // Speed of particles outwards
        pe_ent.part_veltype = PARTICLE_ORIGIN_CENTER;    // Explosion
        pe_ent.wakeup_dist = 768;            // Mid range distance
        pe_ent.wakeup_timer = 1;            // Medium timer (runs all the time)
        pe_ent.spawn_base = 0.1;            // Fast Spawn rate
        pe_ent.spawn_rand = 0;                // Spawn rate randomness
        pe_ent.dpp_name = DPP_KEYRED;        // Red particles from top of key
    }
    //----------------------------------------------------------------------
    // Item - Generic Green Key
    //----------------------------------------------------------------------
    else if (pe_ent.part_style == PARTICLE_STYLE_KEYGREEN) {
        pe_ent.spr_name1 = PART_DOTSML_GREEN;
        pe_ent.spr_name2 = PART_DOTMED_GREEN;
        pe_ent.spr_name3 = PART_DOTSML_YELLOW;
        pe_ent.part_limit = 50;                // High Amount of particles
        pe_ent.part_life = 2;                // Short life time
        pe_ent.part_ofs = '0 0 20';            // Move up to the top of the key
        pe_ent.part_vel = '16 16 8';        // Speed of particles outwards
        pe_ent.part_veltype = PARTICLE_ORIGIN_CENTER;    // Explosion
        pe_ent.wakeup_dist = 768;            // Mid range distance
        pe_ent.wakeup_timer = 1;            // Medium timer (runs all the time)
        pe_ent.spawn_base = 0.1;            // Fast Spawn rate
        pe_ent.spawn_rand = 0;                // Spawn rate randomness
        pe_ent.dpp_name = DPP_KEYGREEN;        // Green particles from top of key
    }
    //----------------------------------------------------------------------
    // Item - Generic Purple Key
    //----------------------------------------------------------------------
    else if (pe_ent.part_style == PARTICLE_STYLE_KEYPURPLE) {
        pe_ent.spr_name1 = PART_DOTSML_PURP;
        pe_ent.spr_name2 = PART_DOTMED_PURP;
        pe_ent.spr_name3 = PART_DOTSML_PURP;
        pe_ent.part_limit = 50;                // High Amount of particles
        pe_ent.part_life = 2;                // Short life time
        pe_ent.part_ofs = '0 0 20';            // Move up to the top of the key
        pe_ent.part_vel = '16 16 8';        // Speed of particles outwards
        pe_ent.part_veltype = PARTICLE_ORIGIN_CENTER;    // Explosion
        pe_ent.wakeup_dist = 768;            // Mid range distance
        pe_ent.wakeup_timer = 1;            // Medium timer (runs all the time)
        pe_ent.spawn_base = 0.1;            // Fast Spawn rate
        pe_ent.spawn_rand = 0;                // Spawn rate randomness
        pe_ent.dpp_name = DPP_KEYPURPLE;    // Purple particles from top of key
    }
    //----------------------------------------------------------------------
    // Item - Generic White Key
    //----------------------------------------------------------------------
    else if (pe_ent.part_style == PARTICLE_STYLE_KEYWHITE) {
        pe_ent.spr_name1 = PART_DOTSML_GREY;
        pe_ent.spr_name2 = PART_DOTMED_GREY;
        pe_ent.spr_name3 = PART_DOTSML_WHITE;
        pe_ent.part_limit = 50;                // High Amount of particles
        pe_ent.part_life = 2;                // Short life time
        pe_ent.part_ofs = '0 0 20';            // Move up to the top of the key
        pe_ent.part_vel = '16 16 8';        // Speed of particles outwards
        pe_ent.part_veltype = PARTICLE_ORIGIN_CENTER;    // Explosion
        pe_ent.wakeup_dist = 768;            // Mid range distance
        pe_ent.wakeup_timer = 1;            // Medium timer (runs all the time)
        pe_ent.spawn_base = 0.1;            // Fast Spawn rate
        pe_ent.spawn_rand = 0;                // Spawn rate randomness
        pe_ent.dpp_name = DPP_KEYWHITE;        // White particles from top of key
    }
    //----------------------------------------------------------------------
    // Item - Sigil / Rune
    //----------------------------------------------------------------------
    else if (pe_ent.part_style == PARTICLE_STYLE_SIGIL) {
        pe_ent.spr_name1 = PART_DOTMED_PURP;
        pe_ent.spr_name2 = PART_DOTSML_PURP;
        pe_ent.spr_name3 = PART_DOTSML_PURP;
        pe_ent.part_limit = 50;                // High Amount of particles
        pe_ent.part_life = 1.5;                // Short life time
        pe_ent.part_ofs = '0 0 16';            // Move up to center of sigil
        pe_ent.part_vel = '8 8 16';            // Speed of particles outwards
        pe_ent.part_veltype = PARTICLE_ORIGIN_CENTER;    // Explosion
        pe_ent.wakeup_dist = 768;            // Mid range distance
        pe_ent.wakeup_timer = 1;            // Medium timer (runs all the time)
        pe_ent.spawn_base = 0.1;            // Fast Spawn rate
        pe_ent.spawn_rand = 0;                // Spawn rate randomness
        pe_ent.dpp_name = DPP_SIGIL;        // Purple particles from center
    }
    //----------------------------------------------------------------------
    // Item - Custom Backpack
    //----------------------------------------------------------------------
    else if (pe_ent.part_style == PARTICLE_STYLE_BACKPACK) {
        pe_ent.spr_name1 = PART_DOTMED_GREY;
        pe_ent.spr_name2 = PART_DOTSML_WHITE;
        pe_ent.spr_name3 = PART_DOTSML_GREY;
        pe_ent.part_limit = 50;                // High Amount of particles
        pe_ent.part_life = 2;                // Short life time
        pe_ent.part_ofs = '0 0 12';            // Middle of pack
        pe_ent.part_vel = '16 16 8';        // Speed of particles outwards
        pe_ent.part_veltype = PARTICLE_ORIGIN_CENTER;    // Explosion
        pe_ent.wakeup_dist = 768;            // Mid range distance
        pe_ent.wakeup_timer = 1;            // Medium timer (runs all the time)
        pe_ent.spawn_base = 0.1;            // Fast Spawn rate
        pe_ent.spawn_rand = 0;                // Spawn rate randomness
        pe_ent.dpp_name = DPP_BACKPACK;        // Grey particles from center
    }
    //======================================================================
    // Artifacts
    //======================================================================
    //----------------------------------------------------------------------
    // Artifact - Environmental Suit
    //----------------------------------------------------------------------
    else if (pe_ent.part_style == PARTICLE_STYLE_SUIT) {
        pe_ent.spr_name1 = PART_DOTSML_GREEN;
        pe_ent.spr_name2 = PART_DOTMED_GREEN;
        pe_ent.spr_name3 = PART_DOTSML_YELLOW;
        pe_ent.part_limit = 75;                // High Amount of particles
        pe_ent.part_life = 2.75;            // Short life time
        pe_ent.part_ofs = '0 0 28';            // Move up to belt of Suit (chest=32)
        pe_ent.part_vel = '8 8 3';            // Speed of particles outwards
        pe_ent.part_velbase = '0 0 12';        // Move upwards
        pe_ent.part_veltype = PARTICLE_ORIGIN_CENTER;    // Explosion
        pe_ent.wakeup_dist = 768;            // Mid range distance
        pe_ent.wakeup_timer = 1;            // Medium timer (runs all the time)
        pe_ent.spawn_base = 0.1;            // Fast Spawn rate
        pe_ent.spawn_rand = 0;                // Spawn rate randomness
        pe_ent.dpp_name = DPP_SUIT;            // Green/Yellow particles from center
        pe_ent.dpp_wait = 0.5;                // Slow timer
    }
    //----------------------------------------------------------------------
    // Artifact - Pentagram
    //----------------------------------------------------------------------
    else if (pe_ent.part_style == PARTICLE_STYLE_PENT) {
        pe_ent.spr_name1 = PART_DOTSML_RED;
        pe_ent.spr_name2 = PART_DOTMED_RED;
        pe_ent.spr_name3 = PART_DOTSML_GOLD;
        pe_ent.part_limit = 75;                // High Amount of particles
        pe_ent.part_life = 2.5;                // Short life time
        pe_ent.part_ofs = '0 0 16';            // Move up to center of Pent
        pe_ent.part_vel = '12 12 12';        // Speed of particles outwards
        pe_ent.part_veltype = PARTICLE_ORIGIN_CENTER;    // Explosion
        pe_ent.wakeup_dist = 768;            // Mid range distance
        pe_ent.wakeup_timer = 1;            // Medium timer (runs all the time)
        pe_ent.spawn_base = 0.1;            // Fast Spawn rate
        pe_ent.spawn_rand = 0;                // Spawn rate randomness
        pe_ent.dpp_name = DPP_PENT;            // Red/Gold particles from center
        pe_ent.dpp_wait = 1;                // Slow timer
    }
    //----------------------------------------------------------------------
    // Artifact - Shadow Ring
    //----------------------------------------------------------------------
    else if (pe_ent.part_style == PARTICLE_STYLE_SRING) {
        pe_ent.spr_name1 = PART_DOTSML_YELLOW;
        pe_ent.spr_name2 = PART_DOTSML_WHITE;
        pe_ent.spr_name3 = PART_DOTSML_GOLD;
        pe_ent.part_limit = 35;                // High Amount of particles
        pe_ent.part_life = 2;                // Short life time
        pe_ent.part_ofs = '0 0 6';            // Move up to the top of the amulet
        pe_ent.part_vel = '8 8 3';            // Speed of particles outwards
        pe_ent.part_velbase = '0 0 -12';    // Move downwards
        pe_ent.part_veltype = PARTICLE_ORIGIN_CENTER;    // Explosion
        pe_ent.wakeup_dist = 768;            // Mid range distance
        pe_ent.wakeup_timer = 1;            // Medium timer (runs all the time)
        pe_ent.spawn_base = 0.1;            // Fast Spawn rate
        pe_ent.spawn_rand = 0.1;            // Spawn rate randomness
        pe_ent.dpp_name = DPP_SRING;        // White/Gold particles from center
    }
    //----------------------------------------------------------------------
    // Artifact - Quad Damage
    //----------------------------------------------------------------------
    else if (pe_ent.part_style == PARTICLE_STYLE_QUAD) {
        pe_ent.spr_name1 = PART_DOTMED_BLUE;
        pe_ent.spr_name2 = PART_DOTSML_WHITE;
        pe_ent.spr_name3 = PART_DOTSML_BLUE;
        pe_ent.part_limit = 75;                // High Amount of particles
        pe_ent.part_life = 1.5;                // Short life time
        pe_ent.part_ofs = '0 0 16';            // Move up to center of quad
        pe_ent.part_vel = '10 10 16';        // Speed of particles outwards
        pe_ent.part_veltype = PARTICLE_ORIGIN_CENTER;    // Explosion
        pe_ent.wakeup_dist = 768;            // Mid range distance
        pe_ent.wakeup_timer = 1;            // Medium timer (runs all the time)
        pe_ent.spawn_base = 0.1;            // Fast Spawn rate
        pe_ent.spawn_rand = 0;                // Spawn rate randomness
        pe_ent.dpp_name = DPP_QUAD;            // Blue/White particles from center
    }
    //----------------------------------------------------------------------
    // Artifact - Tome of Power
    //----------------------------------------------------------------------
    else if (pe_ent.part_style == PARTICLE_STYLE_TOMEOFP) {
        pe_ent.spr_name1 = PART_DOTSML_GOLD;
        pe_ent.spr_name2 = PART_DOTSML_YELLOW;
        pe_ent.spr_name3 = PART_DOTSML_GREY;
        pe_ent.part_limit = 75;                // High Amount of particles
        pe_ent.part_life = 2;                // Short life time
        pe_ent.part_ofs = '0 0 16';            // Lower part of book
        pe_ent.part_vel = '16 16 8';        // Speed of particles outwards
        pe_ent.part_velbase = '0 0 -4';        // Move downward
        pe_ent.part_veltype = PARTICLE_ORIGIN_CENTER;    // Explosion
        pe_ent.wakeup_dist = 768;            // Mid range distance
        pe_ent.wakeup_timer = 1;            // Medium timer (runs all the time)
        pe_ent.spawn_base = 0.05;            // Fast Spawn rate
        pe_ent.spawn_rand = 0.05;            // Spawn rate randomness
        pe_ent.dpp_name = DPP_TOMEOFP;        // Yellow particles
        pe_ent.dpp_wait = 0.5;                // Slow timer
    }
    //----------------------------------------------------------------------
    // Artifact - Sharp Shooter
    //----------------------------------------------------------------------
    else if (pe_ent.part_style == PARTICLE_STYLE_SHARP) {
        pe_ent.spr_name1 = PART_DOTMED_PURP;
        pe_ent.spr_name2 = PART_DOTSML_PURP;
        pe_ent.spr_name3 = PART_DOTSML_GREY;
        pe_ent.part_limit = 75;                // High Amount of particles
        pe_ent.part_life = 1.5;                // Short life time
        pe_ent.part_ofs = '0 0 16';            // Move up to center
        pe_ent.part_vel = '10 10 16';        // Speed of particles outwards
        pe_ent.part_veltype = PARTICLE_ORIGIN_CENTER;    // Explosion
        pe_ent.wakeup_dist = 768;            // Mid range distance
        pe_ent.wakeup_timer = 1;            // Medium timer (runs all the time)
        pe_ent.spawn_base = 0.1;            // Fast Spawn rate
        pe_ent.spawn_rand = 0;                // Spawn rate randomness
        pe_ent.dpp_name = DPP_SHARP;        // Purple particles from center
    }
    //----------------------------------------------------------------------
    // Artifact - Nail Piercer
    //----------------------------------------------------------------------
    else if (pe_ent.part_style == PARTICLE_STYLE_PIERCE) {
        pe_ent.spr_name1 = PART_DOTMED_PURP;
        pe_ent.spr_name2 = PART_DOTSML_PURP;
        pe_ent.spr_name3 = PART_DOTSML_GREY;
        pe_ent.part_limit = 75;                // High Amount of particles
        pe_ent.part_life = 1.5;                // Short life time
        pe_ent.part_ofs = '0 0 16';            // Move up to center
        pe_ent.part_vel = '8 8 4';            // Speed of particles outwards
        pe_ent.part_velbase = '0 0 12';        // Move upwards
        pe_ent.part_veltype = PARTICLE_ORIGIN_CENTER;    // Explosion
        pe_ent.wakeup_dist = 768;            // Mid range distance
        pe_ent.wakeup_timer = 1;            // Medium timer (runs all the time)
        pe_ent.spawn_base = 0.1;            // Fast Spawn rate
        pe_ent.spawn_rand = 0;                // Spawn rate randomness
        pe_ent.dpp_name = DPP_PIERCE;        // Purple particles from center
    }
    //----------------------------------------------------------------------
    // Artifact - Wet Suit
    //----------------------------------------------------------------------
    else if (pe_ent.part_style == PARTICLE_STYLE_WETSUIT) {
        pe_ent.spr_name1 = PART_DOTSML_BLUE;
        pe_ent.spr_name2 = PART_DOTMED_BLUE;
        pe_ent.spr_name3 = PART_DOTSML_WHITE;
        pe_ent.part_limit = 75;                // High Amount of particles
        pe_ent.part_life = 2.75;            // Short life time
        pe_ent.part_ofs = '0 0 28';            // Move up to belt of Suit (chest=32)
        pe_ent.part_vel = '8 8 3';            // Speed of particles outwards
        pe_ent.part_velbase = '0 0 12';        // Move upwards
        pe_ent.part_veltype = PARTICLE_ORIGIN_CENTER;    // Explosion
        pe_ent.wakeup_dist = 768;            // Mid range distance
        pe_ent.wakeup_timer = 1;            // Medium timer (runs all the time)
        pe_ent.spawn_base = 0.1;            // Fast Spawn rate
        pe_ent.spawn_rand = 0;                // Spawn rate randomness
        pe_ent.dpp_name = DPP_WETSUIT;        // Blue/White particles raising up
        pe_ent.dpp_wait = 0.5;                // Slow timer
    }
    //======================================================================
    // Weather System
    //======================================================================
    else if (pe_ent.part_style == PARTICLE_STYLE_WEATHER) {
        pe_ent.part_limit = pe_ent.pemit_source.count;
        pe_ent.part_vel = '0 0 0';
        pe_ent.part_vel_z = pe_ent.pemit_source.speed;
        pe_ent.part_velrand = pe_ent.pemit_source.v_angle;
        pe_ent.wakeup_dist = pe_ent.pemit_source.wakeup_dist;
        pe_ent.wakeup_timer = pe_ent.pemit_source.wakeup_timer;
        pe_ent.spawn_base = 0.1;            // Default timer
        pe_ent.spawn_rand = 0;                // No spawn rate randomness
    }
    //----------------------------------------------------------------------
    // No style defined - ERROR!!!!!
    //----------------------------------------------------------------------
    else {
        dprint("\b[PEMITTER]\b Style no ("); dprint(ftos(pe_ent.part_style));
        dprint(") not valid!!!");
        pe_ent.part_style = PARTICLE_STYLE_EMPTY;    // empty definition
        spawn_marker(pe_ent.origin, SPNMARK_YELLOW);// Visual problem
    }
    
    // return any error conditions; 0=ERROR
    return pe_ent.part_style;
};

//----------------------------------------------------------------------
void(entity pe_ent) misc_particle_off =
{    
    // Double check dealing with a particle emitter?
    if (pe_ent.classtype != CT_PARTICLEEMIT) return;
    if (pe_ent.state == STATE_OFF) return;
    // The particle emitter is still being setup, remember state for later
    if (pe_ent.state == STATE_SETUP) {
        pe_ent.spawnflags = PARTICLE_START_OFF;
        return;
    }

    pe_ent.state = STATE_OFF;
    pe_ent.estate = ESTATE_OFF;
    // Setup a particle explosion on floor circles when they are triggered off
    if (pe_ent.part_style == PARTICLE_STYLE_FCIRCLE) {
        particle_ring(pe_ent.origin, '0 0 4', '4 4 16', pe_ent.part_vol_x+8, pe_ent.part_limit, pe_ent.part_life, 4 ); 
        particle_ring(pe_ent.origin, '0 0 4', '0 0 12', pe_ent.part_vol_x-4, pe_ent.part_limit, pe_ent.part_life, 1 ); 
    }        
};

//----------------------------------------------------------------------
void() misc_particle_use =
{    
    // Have particles been enabled via serverflags?
    if (query_configflag(SVR_PARTICLES) != SVR_PARTICLES) return;
    // If particle still not setup toggle spawnflags instead
    if (self.state == STATE_SETUP) {
        if (self.spawnflags & PARTICLE_START_ON) self.spawnflags = PARTICLE_START_OFF;
        else self.spawnflags = PARTICLE_START_ON;
        return;
    }
    
    // Double check for particle style setup correctly
    if (self.part_style == PARTICLE_STYLE_EMPTY) return;

    // Toggle shooter on/off
    if (self.state == STATE_OFF) {
        self.state = STATE_ON;
        self.estate = ESTATE_ON;

        // Is DP particles active?)
        if (ext_dppart || self.dpp_only && query_configflag(SVR_SPRPARTON) == FALSE && world.sprite_particles == FALSE) {
            // decide between particle types
            if (self.part_style == PARTICLE_STYLE_FCIRCLE) emitdpp_fcircle();
            else if (self.part_style == PARTICLE_STYLE_WEATHER) {
                // Check if particle effects are available from the engine
                if (!ext_dpsnow || !ext_dprain) return;
                emitepp_weather();
            }
            else emitdpp_particle();
        }
        // Pixel goodness particles
        else {
            // Weather effects not supported on Fitz/QS engines
            if (self.part_style == PARTICLE_STYLE_WEATHER) return;
            else emit_particle();
        }
    }
    else misc_particle_off(self);
};

//----------------------------------------------------------------------
void(entity pe_ent) misc_particle_on =
{    
    // Double check dealing with a particle emitter?
    if (pe_ent.classtype != CT_PARTICLEEMIT) return;
    if (pe_ent.state == STATE_ON) return;
    // The particle emitter is still being setup, remember state for later
    if (pe_ent.state == STATE_SETUP) {
        pe_ent.spawnflags = PARTICLE_START_ON;
        return;
    }

    pe_ent.state = STATE_OFF;
    pe_ent.think = misc_particle_use;
    pe_ent.nextthink = time + TIME_MINTICK;
};

//----------------------------------------------------------------------
void(entity pe_ent, float partemit_style) misc_particle_update =
{
    // Double check dealing with a particle emitter?
    if (pe_ent.classtype != CT_PARTICLEEMIT) return;
    if (pe_ent.state == STATE_SETUP) return;
    if (pe_ent.part_style == PARTICLE_STYLE_EMPTY) return;

    pe_ent.part_style = partemit_style;
    if (misc_particle_style(pe_ent) == FALSE) 
        dprint("\b[MISC_PARTICLE]\b Error updating emitter!");
};

//----------------------------------------------------------------------
void() misc_particle_switch_on = { misc_particle_on(self); };
void() misc_particle_switch_off = { misc_particle_off(self); };
void() misc_particle_switch_disable = { self.state = STATE_OFF; };

//----------------------------------------------------------------------
void() misc_particle_finishsetup =
{
    // Was the particle emitter setup correctly?
    if (self.part_style != PARTICLE_STYLE_EMPTY) {
        self.circular_angle = 0;        // Starting circular angle
        self.state = STATE_OFF;            // Initial state
        self.estate = ESTATE_OFF;
        
        // Setup entity state wrapper if targetname exists
        if (self.targetname != "") self.use = entity_state_use;
        else self.use = misc_particle_use;
        self.estate_on = misc_particle_switch_on;
        self.estate_off = misc_particle_switch_off;
        self.estate_disable = misc_particle_switch_disable;
        self.estate_use = misc_particle_use;
        
        // Delay start of particle emitter so if it needs to be switched off
        // other functions can catch it and save switching states quickly
        if (self.spawnflags & PARTICLE_START_ON) misc_particle_use();
    } 
};

//----------------------------------------------------------------------
void() misc_particle_delay =
{
    // If target setup, find it and store entity
    if (self.target != "") self.pemit_target = find(world, targetname, self.target);
    if (!self.pemit_target) self.target = "";
    
    if (!self.start_delay) self.start_delay = 1 + (random()*2);
    self.nextthink = time + self.start_delay;
    self.think = misc_particle_finishsetup;
};

//----------------------------------------------------------------------
void() misc_particletemplate = { 
    self.classtype = CT_PARTICLETEMP; 
    // make sure any sprite assets used are pre-cached ready
    if (self.spr_name1 != "") precache_model(self.spr_name1);
    if (self.spr_name2 != "") precache_model(self.spr_name2);
    if (self.spr_name3 != "") precache_model(self.spr_name3);
};

//----------------------------------------------------------------------
void() misc_particle_copytemplate = 
{
    local entity fdest;

    // Cycle through list for TARGET2 template
    fdest = find (world, targetname, self.target2);
    while(fdest) {
        if (fdest.classtype == CT_PARTICLETEMP) {
            self.owner = fdest;
            fdest = world;
        }
        else fdest = find(fdest, targetname, self.target2);
    }

    // Is the particle template missing?
    if (!self.owner) {
        dprint("\b[MISC_PARTICLE]\b Missing target template!\n");
        spawn_marker(self.origin, SPNMARK_YELLOW);
        return;
    }
    
    // Custom particle setup defined in map
    self.part_style = PARTICLE_STYLE_CUSTOM;
    
    // Copy over all the DEFINED data from the template
    // Fill in any gaps with default data (based on quad)
    if (self.owner.spr_name1) self.spr_name1 = self.owner.spr_name1;
    else self.spr_name1 = PART_DOTMED_BLUE;
    if (self.owner.spr_name2) self.spr_name2 = self.owner.spr_name2;
    else self.spr_name2 = PART_DOTSML_WHITE;
    if (self.owner.spr_name3) self.spr_name3 = self.owner.spr_name3;
    else self.spr_name3 = PART_DOTSML_BLUE;

    // Particle quantity, life, direction, rotation and volume
    if (self.owner.part_movetype) self.part_movetype = self.owner.part_movetype;
    else self.part_movetype = MOVETYPE_NOCLIP;
    if (self.owner.part_limit) self.part_limit = self.owner.part_limit;
    else self.part_limit = 25;
    if (self.owner.part_life) self.part_life = self.owner.part_life;
    else self.part_life = 1;
    if (self.owner.part_ofs) self.part_ofs = self.owner.part_ofs;
    else self.part_ofs = '0 0 0';
    if (self.owner.part_veltype) self.part_veltype = self.owner.part_veltype;
    else self.part_veltype = PARTICLE_ORIGIN_CENTER;
    if (self.owner.part_vel) self.part_vel = self.owner.part_vel;
    if (self.owner.part_velbase) self.part_velbase = self.owner.part_velbase;
    if (self.owner.part_velrand) self.part_velrand = self.owner.part_velrand;
    if (self.owner.part_velrot) self.part_velrot = self.owner.part_velrot;
    if (self.owner.part_vol) self.part_vol = self.owner.part_vol;

    // Particle spawn timing and wakeup distance
    if (self.owner.wakeup_dist) self.wakeup_dist = self.owner.wakeup_dist;
    else self.wakeup_dist = 1024;
    if (self.owner.wakeup_timer) self.wakeup_timer = self.owner.wakeup_timer;
    else self.wakeup_timer = 1;
    if (self.wakeup_timer < TIME_MINTICK) self.wakeup_timer = TIME_MINTICK;
    if (self.owner.spawn_base) self.spawn_base = self.owner.spawn_base;
    else self.spawn_base = 0.1;
    if (self.spawn_base < TIME_MINTICK) self.spawn_base = TIME_MINTICK;
    if (self.owner.spawn_rand) self.spawn_rand = self.owner.spawn_rand;
    else self.spawn_rand = 0;

    // Particle setup for DarkPlaces
    if (self.owner.dpp_name) self.dpp_name = self.owner.dpp_name;
    else self.dpp_name = "";
    if (self.owner.dpp_wait) self.dpp_wait = self.owner.dpp_wait;
    else self.dpp_wait = 0.1;
    if (self.dpp_wait < TIME_MINTICK) self.dpp_wait = TIME_MINTICK;
    if (self.owner.dpp_rnd) self.dpp_rnd = self.owner.dpp_rnd;
    else self.dpp_rnd = 0;
    if (self.owner.dpp_vel) self.dpp_vel = self.owner.dpp_vel;
    else self.dpp_vel = '0 0 0';

    // Setup particle states
    misc_particle_delay();
};

//----------------------------------------------------------------------
void() misc_particle_setup =
{
    // Setup Particle styles (pre-defined)
    if (misc_particle_style(self) != PARTICLE_STYLE_EMPTY) 
        misc_particle_delay();
};

//----------------------------------------------------------------------
// Entry point for code to spawn particle emitter
// source - starting point for particles
// target - destination (can be self/world to spawn at origin)
// style - type of particle to create (check styles above)
// spawnf - spawnflag values (startoff)
//----------------------------------------------------------------------
entity(entity pe_source, entity pe_target, float pe_style, float pe_spawnf) spawn_pemitter =
{
    local entity new_pe;
    
    new_pe = spawn();
    new_pe.classtype = CT_PARTICLEEMIT;
    new_pe.state = STATE_SETUP;
    new_pe.pemit_source = pe_source;
    
    if (pe_source != pe_target) new_pe.pemit_target = pe_target;
    
    // Setup a random number on source entity for linking
    // If the source entity is deleted this number will change
    gen_unique_no(self);
    new_pe.entno_unique = self.entno_unique;

    // Check for bmodel origin first
    if (pe_source.bsporigin) new_pe.origin = bmodel_origin(pe_source);
    else new_pe.origin = pe_source.origin;
    
    // Move Particle emitter to source origin
    setorigin(new_pe, new_pe.origin);
    new_pe.part_style = pe_style;        // must use global defs
    new_pe.spawnflags = pe_spawnf;        // spawnflags are parameters
    new_pe.angles = pe_source.angles;    // Copy over any angle params
    
    // Back through emitter function
    new_pe.think = misc_particle_setup;
    new_pe.nextthink = time + 0.1 + random();
    
    return new_pe;
};

//----------------------------------------------------------------------
void() misc_particle =
{
    if (self.spawnflags & ENT_STARTOFF) self.spawnflags = PARTICLE_START_OFF;
    else self.spawnflags = PARTICLE_START_ON;
    
    // Default setup for any particle emitter
    self.classtype = CT_PARTICLEEMIT;
    self.pemit_source = self;
    gen_unique_no(self);
    self.state = STATE_SETUP;
    // Let state of particle emitter change before finishing setup
    if (self.targetname != "") self.use = misc_particle_use;
    
    // Custom particle setup using template?
    if (self.target2 != "") { 
        self.nextthink = time + 0.1 + random();
        self.think = misc_particle_copytemplate;
        return; 
    }

    // Setup any pre-defined particle types
    if (self.message == "ARMOR1") { self.part_style = PARTICLE_STYLE_ARMOR; self.items2 = IT2_ARMOR1; }
    else if (self.message == "ARMOR2") { self.part_style = PARTICLE_STYLE_ARMOR; self.items2 = IT2_ARMOR2; }
    else if (self.message == "ARMOR3") { self.part_style = PARTICLE_STYLE_ARMOR; self.items2 = IT2_ARMOR3; }

    else if (self.message == "BOOK") self.part_style = PARTICLE_STYLE_BOOK;
    else if (self.message == "ELECTRIC") self.part_style = PARTICLE_STYLE_ELECTRIC;
    else if (self.message == "FCIRCLE") self.part_style = PARTICLE_STYLE_FCIRCLE;
    else if (self.message == "FLAMES") self.part_style = PARTICLE_STYLE_FLAMES;
    else if (self.message == "FLAMEL") self.part_style = PARTICLE_STYLE_FLAMEL;
    
    else if (self.message == "KEYGOLD") self.part_style = PARTICLE_STYLE_KEYGOLD;
    else if (self.message == "KEYSILVER") self.part_style = PARTICLE_STYLE_KEYSILVER;
    else if (self.message == "KEYRED") self.part_style = PARTICLE_STYLE_KEYRED;
    else if (self.message == "KEYGREEN") self.part_style = PARTICLE_STYLE_KEYGREEN;
    else if (self.message == "KEYPURPLE") self.part_style = PARTICLE_STYLE_KEYPURPLE;
    else if (self.message == "KEYWHITE") self.part_style = PARTICLE_STYLE_KEYWHITE;
    else if (self.message == "MEGAH") self.part_style = PARTICLE_STYLE_MEGAH;
    else if (self.message == "PENT") self.part_style = PARTICLE_STYLE_PENT;
    else if (self.message == "PORTAL") self.part_style = PARTICLE_STYLE_PORTAL;

    else if (self.message == "JUMPPAD") self.part_style = PARTICLE_STYLE_JUMPPAD;
    else if (self.message == "QUAD") self.part_style = PARTICLE_STYLE_QUAD;
    
    else if (self.message == "SIGIL") self.part_style = PARTICLE_STYLE_SIGIL;
    else if (self.message == "ALTAR") self.part_style = PARTICLE_STYLE_ALTAR;
    else if (self.message == "SKILL") self.part_style = PARTICLE_STYLE_SKILL;
    else if (self.message == "SRING") self.part_style = PARTICLE_STYLE_SRING;
    else if (self.message == "SUIT") self.part_style = PARTICLE_STYLE_SUIT;
    else {
        dprint("\b[MISC_PARTICLE]\b Missing particle style!\n");
        spawn_marker(self.origin, SPNMARK_YELLOW);
        return;
    }
    
    // Back through emitter function
    self.nextthink = time + 0.1 + random();
    self.think = misc_particle_setup;
};

//----------------------------------------------------------------------
void() misc_particle_burst_use =
{
    particle_explode(self.origin+self.part_ofs, self.count, self.part_life, self.part_style, self.part_movetype);
    // Check for temporary continuous mode
    if (self.waitmin > 0) {
        if (self.pausetime == 0) {
            if (!self.waitmin2) self.waitmin2 = 0.1;
            self.pausetime = time + self.waitmin;
        }
        if (self.pausetime > time) {
            self.think = misc_particle_burst_use;
            self.nextthink = time + self.waitmin2;
        }
        else self.pausetime = 0;
    }
};

//----------------------------------------------------------------------
// This is also used by item_custom in items.qc
//----------------------------------------------------------------------
void() misc_particle_burst_setup =
{
    // Read tcount from entity, but it cannot be used for constant emitters
    // The finish particle system will decrease the tcount directly!
    if (!self.pemit_tcount) self.count = 20;
    
    if (!self.part_life) self.part_life = 2;
    if (CheckZeroVector(self.part_ofs)) self.part_ofs = '0 0 0';

    if (self.part_style == 1) self.part_style = PARTICLE_BURST_YELLOW;
    else if (self.part_style == 2) self.part_style = PARTICLE_BURST_GREEN;
    else if (self.part_style == 3) self.part_style = PARTICLE_BURST_RED;
    else if (self.part_style == 4) self.part_style = PARTICLE_BURST_BLUE;
    else if (self.part_style == 5) self.part_style = PARTICLE_BURST_PURPLE;
    else if (self.part_style == 6) self.part_style = PARTICLE_BURST_FIRE;
    else self.part_style = PARTICLE_BURST_WHITE;
    
    if (self.part_movetype == 3) self.part_movetype = PARTICLE_BURST_UPWARD;
    else if (self.part_movetype == 4) self.part_movetype = PARTICLE_BURST_SHOCKWAVE;
    else if (self.part_movetype == 5) self.part_movetype = PARTICLE_BURST_SKULLUP;
    else if (self.part_movetype == 6) self.part_movetype = PARTICLE_BURST_LOSTUP;
    else if (self.part_movetype == 7) self.part_movetype = PARTICLE_BURST_MINOTAUR;
    else self.part_movetype = PARTICLE_BURST_CENTER;
};

//----------------------------------------------------------------------
void() misc_particle_burst =
{
    // Default setup for any particle emitter
    self.classtype = CT_PARTICLEEMIT;
    self.pemit_source = self;
    self.health = LARGE_TIMER;
    
    misc_particle_burst_setup();
    self.use = misc_particle_burst_use;
};

//----------------------------------------------------------------------
void() misc_weather =
{
    // Setup particle emitter to start on/off
    if (self.spawnflags & ENT_STARTOFF) 
        self.spawnflags = self.spawnflags | PARTICLE_START_OFF;
    else self.spawnflags = self.spawnflags | PARTICLE_START_ON;
    
    // Default setup for any particle emitter
    self.classtype = CT_PARTICLEEMIT;
    self.part_style = PARTICLE_STYLE_WEATHER;
    self.pemit_source = self;
    gen_unique_no(self);
    self.state = STATE_SETUP;
    
    // Find out size of volume brush
    InitTrigger();
    // Save the brush bounding box for later
    self.bbmins = self.mins;
    self.bbmaxs = self.maxs;
    self.oldorigin = bmodel_origin(self);
    spawn_marker(self.oldorigin,0);
    
    // hide volume, no longer needed
    self.solid = SOLID_NOT;
        
    // Setup default particle count based on volume
    // - 10% of the volume size as particle count
    if (!self.count) 
        self.count = rint((vlen(self.bbmaxs - self.bbmins)*0.1));
    
    // Change for variable wind change
    if (!self.speed) self.speed = 30;
    self.attack_speed = time + self.speed + (random()*self.speed);
    
    // Setup default wind (X/Y) and vertical velocity
    if (self.spawnflags & PARTICLE_WEATHER_SNOW) {
        if (CheckZeroVector(self.pos1)) self.pos1 = '8 15 0'; // Mostly white
        if (CheckZeroVector(self.pos2)) self.pos2 = '100 100 -100';
    }
    else {
        if (CheckZeroVector(self.pos1)) self.pos1 = '1 15 0'; // grey->white
        if (CheckZeroVector(self.pos2)) self.pos2 = '100 100 -500';
    }

    // Particle colour range - Make sure Y > X
    if (self.pos1_x > self.pos1_y) {
        self.pos1_z = self.pos1_x;
        self.pos1_x = self.pos1_y;
        self.pos1_y = self.pos1_z;
    }
    // Work out range of colour
    self.pos1_z = rint(self.pos1_y - self.pos1_x);
    if (self.pos1_z < 1) self.pos1_z = 1;
    
    // Create initial wind direction
    self.pos3_x = crandom() * self.pos2_x;
    self.pos3_y = crandom() * self.pos2_y;
    self.pos3_z = self.pos2_z;
    
    // Back through emitter function
    self.nextthink = time + 0.1 + random();
    self.think = misc_particle_setup;
};

/*======================================================================
/*QUAKED misc_particle (0 0.5 0.75) (-8 -8 -8) (8 8 8) x x x x x x STARTOFF x
A particle emitter which can be turned on/off (use function)
-------- KEYS --------
targetname   : toggle state (use trigger ent for exact state)
target       : destination of effect (self -> target)
target2      : name of a particle template
message      : Particle style string (upper case) leave blank for custom
               ARMOR1, ARMOR2, ARMOR3, BOOK, ELECTRIC, FCIRCLE, 
               FLAMES, FLAMEL, KEYGOLD, KEYSILVER, MEGAH, PENT,
               PORTAL, QUAD, SIGIL, SKILL, SRING, SUIT
-------- SPAWNFLAGS --------
STARTOFF : Always Starts off and waits for trigger
-------- NOTES --------
A particle emitter which can be turned on/off (use function)

//-----------------------------------------------------------------------------
/*QUAKED misc_particletemplate (0 0.5 0.75) (-8 -8 -8) (8 8 8) x
A particle emitter template
-------- KEYS --------
targetname   : target name reference for misc_particle entities
start_delay  : Particle emitter start delay
spr_name1    : sprite name (progs\s_bubble_blue1.spr)
spr_name2    : sprite name (progs\s_bubble_wht.spr)
spr_name3    : sprite name (progs\ad171\s_dotmed_grey.spr)
part_movetype: Movetype of particle (def.qc for movetypes)
part_limit   : Maximum amount of particles to emit at once
part_life    : Life time particle is active (seconds)
part_ofs     : Offset from emitter (XYZ)
part_veltype : Velocity type 1=random, 2=circular
part_vel     : Velocity direction (XYZ)
part_velrand : Extra random velocity (need vel type 1)
part_velrot  : Velocity rotation (Y axis only)
part_vol     : Spawning volume around emitter (XYZ)
cirular_angle: Circular rotation angle
wakeup_dist  : Wake up distance for particle emitter
wakeup_timer : How often to check distance check
spawn_base   : Spawn rate - base value (seconds)
spawn_rand   : Spawn rate - random adjustment (seconds)
dpp_name     : DP particle effect name in effectinfo.txt file
dpp_wait     : DP particle re-trigger timer
dpp_rnd      : DP particle random multiplier for time
dpp_vel      : Direction of DP particles to move towards (XYZ)
-------- SPAWNFLAGS --------
-------- NOTES --------
A particle emitter template

//-----------------------------------------------------------------------------
/*QUAKED misc_particle_burst (0 0.5 0.75) (-8 -8 -8) (8 8 8) x
A particle emitter that fires a burst
-------- KEYS --------
targetname    : Trigger Particle Emitter
part_ofs      : Particle Origin Offset (def='0 0 0')
part_tcount   : Particle Quantity (def=20)
part_life     : Particle Life Time (def=2s)
part_style    : 1=yellow, 2=green, 3=red, 4=blue, 5=purple, 6=fire, 7=white
part_movetype : 2=center, 3=up, 4=shockwave, 5=skull, 6=lost, 7=minotaur
-------- SPAWNFLAGS --------
-------- NOTES --------
A particle emitter that fires a burst

//-----------------------------------------------------------------------------
/*QUAKED misc_weather (0 0.5 0.75) ? x x SILENT SNOW BLOOD SLIME STARTOFF x
A particle emitter which produces weather effects
-------- KEYS --------
targetname : toggle state (use trigger ent for exact state)
count      : Quantity of particles for volume
speed      : Downward velocity (def=300)
v_angle    : Random velocity adjustment (def=8 8 150)
wakeup_dist  : how close the player needs to be for the weather to work
wakeup_timer : time to keep checking when in standby mode
-------- SPAWNFLAGS --------
SILENT   : Don't make any drip sound (good for multiple drips)
SNOW     : White wispy flakes falling
BLOOD    : Blood red rain
SLIME    : Slime green rain
STARTOFF : Always Starts off and waits for trigger
-------- NOTES --------
A particle emitter which produces weather effects

======================================================================*/
