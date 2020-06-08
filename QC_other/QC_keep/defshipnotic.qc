//======================================================================
// MOD    : Hipnotic (Ritual Entertainment)
// Kept by the Keep Mod.  You keep what you kill (and kill with).
//======================================================================
float AS_DODGING = 9;// was 5 in hipnotic originally
float MIN_ANGLE_DELTA    = 10.000;
float UNDERWATER    = 2.000;
float CHARMED_RADIUS    = 1500.000;
float MAX_CHARMER_DISTANCE    = 200.000;
float MIN_CHARMER_DISTANCE    = 150.000;
float TOOCLOSE_CHARMER_DISTANCE    = 120.000;
float hipearthquake;
.float horn_active;
.entity charmer;
.float charmed;
.float huntingcharmer;
float quakeactive;
float tesla_numtargets;
//.entity horn_charmer;
entity tesla_target;
.vector old_velocity;
.entity lastvictim;
.entity next_ent;
.entity spawnmaster;
//.float ammo_cells1;
.float col;
.float empathy_finished;
//.float empathy_sound;
.float empathy_time;
.float gorging;
.float spawnsilent;
.float spawnmulti;
.float stoleweapon;
.float struck_by_mjolnir;
.float wetsuit_finished;
.float wetsuit_time;
float discharged;
float NumProximityGrenades;
float visible_distance;
float modelindex_hammer;


