void() precache_poisongibs;

void() PrecacheAll = {
    precache_sound ("hipitems/empathy2.wav");    // Empathy shield sound
    precache_sound ("misc/wetsuit.wav");        // Wetsuit noise
    precache_sound ("shield/hit.wav");        // Power Shield hit
    precache_sound ("shield/fadeout.wav");        // Power Shield fadeout
    precache_sound ("belt/use.wav");        // Antigrav ping
    precache_sound ("belt/fadeout.wav");        // Antigrav fadeout
    precache_sound ("pyro/fstop.wav");        // For dragon fire
    precache_sound ("pyro/flame.wav");        // For dragon fire
    precache_sound ("weapons/crossbow_fire.wav");
    precache_sound ("hipweap/laserric.wav");
    precache_sound ("hipweap/laserg.wav");
    precache_sound ("hipweap/mjolslap.wav");
    precache_sound ("hipweap/mjolhit.wav");
    precache_sound ("hipweap/mjoltink.wav");
    precache_sound ("hipweap/proxwarn.wav");
    precache_sound ("hipweap/proxbomb.wav");
    precache_sound ("weapons/crossbow_rdy.wav");
    precache_sound ("spider/hiss2.wav");
    precache_sound ("lavagun/snail.wav");
    precache_sound ("boss1/sight1.wav");
    precache_sound ("plasma/explode.wav");
    precache_sound ("plasma/fire.wav");
    precache_sound ("plasma/flight.wav");
    precache_sound ("items2/trinend.wav");
    precache_sound ("items2/crosend.wav");
    precache_sound ("items2/crosuse.wav");
    precache_sound ("items/inv3.wav");
    precache_sound ("items/inv2.wav");
    precache_sound ("items/suit2.wav");
    //precache_sound ("items/suit3.wav");
    precache_sound ("items/wetsuit3.wav");
    precache_sound ("items/wetsuit3out.wav");
    precache_sound ("items/wetsuit2.wav");
    precache_sound ("items/damage2.wav");
    precache_sound ("items/sharp2.wav");
    precache_sound ("items/protect2.wav");
    precache_sound ("items/nailp2.wav");
    precache_sound ("items/sharp3.wav");
    precache_sound ("nour/idle1.wav");
    precache_sound ("weapons/expl1.wav");
    precache_sound ("weapons/shatter.wav");
    precache_sound ("cube/bc_gulp.wav");
    precache_sound ("cube/bc_attck.wav");
    precache_sound ("gibfnt/gf_plop.wav");
    precache_sound ("player/udeath.wav");
    precache_sound ("boss1/out1.wav");
    precache_sound ("nemesant/bigboom.wav");
    precache_sound ("weapons/sawguts.wav");
    precache_sound ("weapons/r_exp3a.wav");
    precache_sound ("items/money.wav");
    precache_sound ("ambience/thunder1.wav");
    precache_sound ("ambience/thunder2.wav");
    precache_sound ("weapons/axhit.wav");
    precache_sound ("items/levitate.wav");
    precache_sound ("ambience/wingend.wav");
    precache_sound ("shambler/smack.wav");
    precache_sound ("weapons/sawguts.wav");
    precache_sound ("weapons/chain1.wav");
    precache_sound ("items/reflect3.wav");
    precache_sound ("zombie/z_hit.wav");
    precache_sound ("weapons/stunfire.wav");
    precache_sound ("weapons/sawidle.wav");
    precache_sound ("weapons/sawatck.wav");
    precache_sound ("weapons/sawridle.wav");
    precache_sound ("weapons/clang.wav");
    precache_sound ("weapons/axhitwal.wav");
    precache_sound ("weapons/bolt_hit1.wav");
    precache_sound ("weapons/bolt_hit4.wav");
    precache_sound ("enforcer/enfire.wav");
    precache_sound ("xmas/raindeer/hit.wav");

    precache_model ("progs/null.mdl");
    //precache_model ("progs/drake/null_256.spr");
    precache_model ("progs/beamhit.spr");
    precache_model ("progs/custents/dring.spr");
    precache_model ("progs/k_spike.mdl");
    precache_model ("progs/drake/s_explod.spr");
    precache_model ("progs/smlexp.spr");
    precache_model ("progs/medexp.spr");    // For dragon fire
    precache_model ("progs/hipnotic/lasrspik.mdl");
    precache_model ("progs/hipnotic/proxbomb.mdl");
    precache_model ("progs/g_crossbow.mdl");
    precache_model ("progs/v_crossbow1.mdl");
    precache_model ("progs/v_crossbow2.mdl");
    precache_model ("progs/hipnotic/v_prox.mdl");
    precache_model ("progs/v_plasma2.mdl");
    precache_model ("progs/v_lava.mdl");
    precache_model ("progs/v_lava2.mdl");
    precache_model ("progs/v_multi.mdl");
    precache_model ("progs/v_multi2.mdl");
    precache_model ("progs/hipnotic/v_laserg.mdl");
    precache_model ("progs/hipnotic/v_hammer.mdl"); //mjolnir
    precache_model ("progs/hipnotic/playham.mdl"); // player with the hammer
    precache_model ("progs/v_ham.mdl"); //warhammer from quoth
    precache_model ("progs/drake/v_wand.mdl");
    precache_model ("progs/ad171/proj_bolt1.mdl");
    precache_model ("progs/ad171/proj_bolt2.mdl");
    precache_model ("progs/lspike.mdl");
    precache_model ("progs/rogue/mervup.mdl");
    precache_model ("progs/rockup_d.mdl");
    precache_model ("progs/rockup.mdl");
    precache_model ("progs/plasma.mdl");
    precache_model ("progs/h_shal.mdl");
    precache_model ("progs/drake/ice_cube.mdl");
    precache_model ("progs/drake/iceball.mdl");
    precache_model ("progs/zom_gib.mdl");
    precache_model ("progs/drake/gr_gib.mdl");
    precache_model ("progs/drake/pur_gib.mdl");
    precache_model ("progs/drake/ember.mdl");
    precache_model ("progs/drake/s_spark.spr");
    precache_model ("progs/drake/acidbal1.mdl");
    precache_model ("progs/drake/s_fancy.spr");
    precache_model ("progs/drake/beam.mdl");      // Used by TE_BEAM.
    precache_model ("progs/drake/hook.mdl");
    precache_model ("progs/drake/v_grpple.mdl");
    precache_model ("progs/drake/v_light2.mdl");
    precache_model ("progs/drake/v_rocks.mdl");
    precache_model ("progs/drake/v_laserb.mdl");
    precache_model ("progs/drake/v_laserg.mdl");
    precache_model ("progs/drake/v_saw.mdl");
    precache_model ("progs/drake/playham.mdl");
    precache_model ("progs/drake/playham0.mdl");
    precache_model ("progs/drake/playsaw.mdl");
    precache_model ("progs/custents/beam.mdl"); // phase beam
    precache_model ("progs/ad171/proj_laz.mdl");
    precache_model ("progs/ad171/proj_gaplasmagrn.mdl");
    precache_model ("progs/ad171/proj_lightning2.mdl");
    precache_model ("progs/ad171/proj_grenadegrn.mdl");
    precache_model ("progs/ad171/proj_swampling.mdl");
    precache_poisongibs();
};