//======================================================================
// MOD    : Quoth 2.1
// Kept by the Keep Mod.  You keep what you kill (and kill with).
//======================================================================
.void() th_novis;
vector VEC_HULL_MIN = '-16 -16 -24';
vector VEC_HULL_MAX = '16 16 32';
.vector oldenemyorigin;
.vector spawnmaxs;
.entity eye;
.float activated_time;
.float awake;
.float carry_time;
.float coiled;
.float cross_finished;
.float cross_sound;
.float cross_start;
.float cross_time;
.float dangle;
.float dlord_firecnt;
.float dlord_searchtime;
.float dlord_playervis;
.float drole_searchtime;
.float drop;
.float gotbomb;
.float gug_slapped;
.float inv;
.float multiplier;
.float polyp_speed;
.float shield;
.float startonground;
.float statevis;
.float trinity_finished;
.float trinity_start;
.float trinity_time;
.float vermis_grabbed;