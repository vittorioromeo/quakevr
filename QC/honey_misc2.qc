


/**************************************************************************/
/* Major changes not in this file:                                        */
/*                                                                        */
/* Combat system has been changed to support specific                     */
/* damage types by player weapons.                                        */
/* Probably every entity with takedamage set can now                      */
/* also set self.flags | FL_SPECIFICDAMAGE (8192) and then                */
/* self.items = IT_<any weapon> to filter damage received                 */
/* as from only those weapons.                                            */
/* Objects when shot with the incorrect weapon will behave                */
/* as a normal wall. All instances I can think of have been               */
/* covered.                                                               */
/* If needed, you can check the damage type given in th_pain, th_die etc  */
/* using self.damage_weapon or damage_attacker.damage_weapon.             */
/* This has so far only been tested on buttons and monsters, and there    */
/* are guaranteed to be places where this doesn't work or works           */
/* unexpectedly. Use with care. Also it's a hack.                         */
/* T_Damage now returns either TRUE or FALSE depending on if the hit      */
/* target should bleed or not.                                            */
/*                                                                        */
/*                                                                        */
/*                                                                        */
/*                                                                        */
/**************************************************************************/


/**************************************************************************/
/* misc_tree                                                              */
/* A tree, the model comes from Quoth, I added a second skin              */
/**************************************************************************/

void() misc_tree =
{
    precache_model("progs/tree1.mdl");
    setmodel(self, "progs/tree1.mdl");

    makestatic(self);
}

/**************************************************************************/
/* info_stuffcmd                                                          */
/**************************************************************************/

void() info_stuffcmd_do =
{
    entity isc;
    isc = find(world, classname, "info_stuffcmd");
    while(isc){
        stuffcmd(self, isc.message);
        stuffcmd(self, "\n");
        isc = find(isc, classname, "info_stuffcmd");
    }
}

/*QUAKED info_stuffcmd (0 .5 .8) ?
    puts the specified message into the console when the client spawns.

*/
void() info_stuffcmd =
{
    if(self.message == "") SUB_Remove();
}

/**************************************************************************/
/* trigger_settarget                                                      */
/**************************************************************************/

void() trigger_settarget_use =
{
    entity t;
    t = find(world, targetname, self.target);
    while(t){
        t.target = self.message;
        t = find(t, targetname, self.target);
    }
    SUB_Remove();
}

/*QUAKED trigger_settarget (0 .5 .8) ?
    When triggered this enetity changes the target field of the target entity to its
    own message field. Useful for path_corners or trigger_teleports

*/
void() trigger_settarget =
{
    if(!self.target)
    {
        objerror("setTarget with no Target!\n");
        SUB_Remove();
        return;
    }

    if(!self.message)
    {
        objerror("setTarget with no Message!\n");
        SUB_Remove();
        return;
    }

    self.use = trigger_settarget_use;
}

/**************************************************************************/
/* trigger_setmessage                                                      */
/**************************************************************************/

void() trigger_setmessage_use =
{
    entity t;
    t = find(world, targetname, self.target);
    while(t){
        t.message = self.message;
        t = find(t, targetname, self.target);
    }
    SUB_Remove();
}

/*QUAKED trigger_setmessage (0 .5 .8) ?
    When triggered this enetity changes the target field of the target entity to its
    own message field. Useful for path_corners or trigger_teleports

*/
void() trigger_setmessage =
{
    if(!self.target)
    {
        objerror("setMessage with no Target!\n");
        SUB_Remove();
        return;
    }

    if(!self.message)
    {
        objerror("setMessage with no Message!\n");
        SUB_Remove();
        return;
    }

    self.use = trigger_setmessage_use;
}

/**************************************************************************/
/* func_refwall                                                           */
/**************************************************************************/

float REFWALL_NOSOLID = 1;

/*QUAKED func_refwall (0 .5 .8) ?
This is just a solid wall that grabs model from a pre-existing func_wall or external bmodel

Doesn't work very well.

Fuck this dumb shit forever!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
*/
void() func_refwall =
{
    entity ref;
    string myModel;
    vector refOrigin;

    if(self.target != NULLSTR)
    {
        ref = find(world, targetname, self.target);
        if(!ref)
        {    objerror("Warning: RefWall without valid reference! (target = ");
            objerror(self.target);
            objerror(")\n");
            SUB_Remove();
            return;
        }
        refOrigin = ref.pos1;
        myModel = ref.model;

    }else if(self.model != NULLSTR){
        refOrigin = '0 0 0';
        myModel = self.model;
        precache_model(self.model);

    }else{
        SUB_Remove();
        return;
    }


    self.origin = self.origin - refOrigin;

    if(self.spawnflags & REFWALL_NOSOLID)
    {
        self.movetype = MOVETYPE_NONE;
        self.solid = SOLID_NOT;
    }else{
        self.movetype = MOVETYPE_PUSH;    // so it doesn't get pushed by anything
        self.solid = SOLID_BSP;
    }

    self.use = func_wall_use;
    setmodel(self, myModel);
    setorigin(self, self.origin);
}


/**************************************************************************/
/* Aimhelp for rockets                                                    */
/**************************************************************************/


vector missile_check_aimhelp(entity client, vector morg, vector defaultvec) =
{
    entity aimhelp;
    float dist;
    vector aimPoint;

    //TODO: atm just returns the first valid aimhelp, but should maybe check if there are multiple and choose the best one
    //TODO: also should make sure the original aim vector wouldn't have hit a monster or something


    //don't do anything if sv_aim >= 3;
    if(cvar_hget(cvarh_sv_aim) >= 3)
        return defaultvec;

    //for each aimhelp
    aimhelp = find(world, classname, "info_aimhelp_rocket");
    while(aimhelp)
    {

        //if close enough to player, but not too close
        dist = vlen(morg - aimhelp.origin);
        if(dist < aimhelp.distance && dist > aimhelp.volume) //160 = radiusdamage done by rockets
        {
            //if can trace from player to aimhelp
            traceline (morg, aimhelp.origin, FALSE, client);
            if(trace_fraction == 1)
            {
                //multiply normalized aiming vector with distance and add to origin to get equivalent point
                aimPoint = morg + (defaultvec * dist);

                //get distance from aimed at point to aimhelp origin
                dist = vlen(aimPoint - aimhelp.origin);

                //if that is less than some distance
                if(dist < aimhelp.lip){
                    //return aim at aimhelp instead of aiming vector.
                    return normalize(aimhelp.origin - morg);
                }
            }
        }
        aimhelp = find(aimhelp, classname, "info_aimhelp_rocket");
    }

    //no good aimhelp found, just return.
    return defaultvec;
}

/*QUAKED info_aimhelp_rocket (0 0.5 0) (-4 -4 -4) (4 4 4)
Used as a helpful aim target for rockets.
*/
void() info_aimhelp_rocket =
{
    if(!self.distance)
        self.distance = 768; //Max distance
    if(!self.volume)
        self.volume = 160; //Min distance

    if(!self.lip)
        self.lip = 32; //Maximum path correction in distance aimed away from self, not degrees

};






/**************************************************************************/
/* The deathpit and everything that entails                               */
/**************************************************************************/


//float deathpit_getDepthFraction(entity pit, vector point);
void() PlayerLingerInPit =
{
    //fog_releaseOverride(self);
    fog_setOverride(self, '0 0 0', 16, 0.0 , 10);
}

void() PlayerHitPitBottom =
{
    //TODO: play a better crunchy sound.
    sound(self, CHAN_VOICE, "player/land2.wav", 1, ATTN_NORM);


    self.items = self.items - (self.items & IT_INVISIBILITY);
    self.invisible_finished = 0;    // don't die as eyes
    self.invincible_finished = 0;
    self.super_damage_finished = 0;
    self.radsuit_finished = 0;
    self.modelindex = modelindex_player;    // don't use eyes
    self.solid = SOLID_NOT;

    //self.weaponmodel="";
    self.view_ofs = '0 0 -8';


    self.state = 0;

    self.angles_x = 0;
    self.angles_z = 0;

    fog_releaseOverride(self);
    fog_setOverride(self, '1 0 0', 16, 1.0, 0.1);
    ClientObituary(self, self.enemy);
    self.deadflag = DEAD_DEAD;
    self.health = -45 - (30*random());

    self.nextthink = time + 1;
    self.think = PlayerLingerInPit;
}

void(entity client) PlayerStartFallInPit =
{
    entity oself;
    if(client.state == STATE_FALLING) return;

    client.state = STATE_FALLING;
    //TODO: play falling sound?
    sound(other, CHAN_VOICE, "ambience/windfly.wav", 1, ATTN_NORM);

    centerprint(client, "\nOh no!\n");

    client.weaponmodel="";
    client.nextthink = -1;
    client.deadflag = DEAD_DYING;
    //client.solid = SOLID_NOT;
    client.flags = client.flags - (client.flags & FL_ONGROUND);
    client.movetype = MOVETYPE_TOSS;

    client.enemy = self.owner;
    //client.attacker = self;

    oself = self;
    self = client;
    SUB_DoLater(PlayerHitPitBottom, 1.5 + (random() * 2.5));
    self = oself;

};

float(entity pit, vector point) deathpit_getDepthFraction{
    //1 = at top, 0 = at bottom
    float depth, min_z, max_z;
    max_z = pit.height;
    min_z = pit.lip;

    if(max_z <= point_z) depth = 1; //Above the pit.
    if(min_z >= point_z) depth = 0; //Below the pit.
    else depth =  (point_z - min_z) / (max_z - min_z);

    //dprint_str_f_str("depth fraction = ", depth, "\n");
    //dprint_str_f_str("height = ", point_z, "\n");
    return depth;
}

void() deathpit_checkplayer_think =
{
    float depth;
    vector o;

    depth = deathpit_getDepthFraction(self.owner, self.enemy.origin);
    if(depth <= 1){ //FUCK YOU FUUUUUUCK
        depth = 1 - depth;
        depth = 1 - (depth * depth);
    }
    fog_setOverrideFraction(self.enemy, 1 - depth);

    if(depth <= 0){
        //Someone's gonna die, so let's just die with him
        remove(self);
        return;

    }else if(depth <= 0.65){
        if(self.enemy.flags & FL_GODMODE) {
            o = self.enemy.origin;
            o_z = 0;
            setorigin(self.enemy, o);
            stuffcmd(self.enemy, "\nnoclip\n");
            centerprint(self.enemy,"Saved by the godmode!\n");
            self.enemy.trigger_field = world;
            //fog_setOverrideFraction(self.enemy, 0);
            fog_releaseOverride(self.enemy);
            remove(self);
            return;
        }else{

            PlayerStartFallInPit(self.enemy);
        }
    }else if(depth >= 1){
        fog_releaseOverride(self.enemy);
        self.enemy.trigger_field = world;
        remove(self);
        return;
    }
    self.nextthink = time + 0.02;

}


void deathpit_checkmonster_think() =
{
    float depth;

    depth = deathpit_getDepthFraction(self.owner, self.enemy.origin);
    if(depth <= 0.1) {

        Killed(self.enemy, self.owner);
        remove(self.enemy);
        remove(self);
        return;
    }

    self.nextthink = time + 0.1;
}

void deathpit_MissileTouch() =
{
    float depth;
    depth = deathpit_getDepthFraction(self.groundentity, self.origin);

    if(depth < 0.1){ //Pretty deep, so we won't explode
        //SUB_DoLater(SUB_Remove, 0.1);
        SUB_Remove();
        return;
    }else{
        T_MissileTouch();
    }
}

void deathpit_GrenadeTouch() =
{
    float depth;
    depth = deathpit_getDepthFraction(self.groundentity, self.origin);


    if(depth < 0.1){ //Pretty deep, so we won't explode
        //SUB_DoLater(SUB_Remove, 0.1);
        SUB_Remove();
        return;
    }else{
        GrenadeTouch();
    }
}

void() ZombieGrenadeTouch;
void() deathpit_ZombieGrenadeTouch =
{
    float depth;
    depth = deathpit_getDepthFraction(self.groundentity, self.origin);


    if(depth < 0.1){ //Pretty deep, so we won't explode
        //SUB_DoLater(SUB_Remove, 0.1);
        SUB_Remove();
        return;
    }else{
        ZombieGrenadeTouch();
    }
};


void deathpit_touch() =
{
    entity tmp;
    float depth;

    if(other.classname == "player"){
        if(other.trigger_field) return;

        //player touch: Start to fall down
        tmp = spawn();
        tmp.enemy = other;
        tmp.owner = self;
        other.trigger_field = tmp;
        tmp.think = deathpit_checkplayer_think;
        tmp.nextthink = time + 0.1;
        self = tmp;
        fog_setOverrideConditional(self.enemy,'0 0 0', 3, 0);

    }else if(other.classname == "missile"){
        //rocket touch: replace the missile touch function
        other.groundentity = self;
        other.touch = deathpit_MissileTouch;


    }else if(other.classname == "grenade"){
        //grenade touch: replace the grenade touch function
        other.groundentity = self;
        other.touch = deathpit_GrenadeTouch;


    }else if(other.classname == "zgrenade"){
        //zombie gib touch: replace the zombie gib touch function
        other.groundentity = self;
        other.touch = deathpit_ZombieGrenadeTouch;


    }else if(other.flags & FL_MONSTER){

        if(other.trigger_field) return;

        //monster touch: silently kill the monster once it reaches the bottom
        tmp = spawn();
        other.trigger_field = tmp;
        tmp.enemy = other; //monster
        tmp.owner = self;  //pit
        tmp.think = deathpit_checkmonster_think;
        tmp.nextthink = time + 0.1;

    }else{
        depth = deathpit_getDepthFraction(self, other.origin);
        if(depth < 0.1){
            self = other;
            SUB_DoLater(SUB_Remove, 0.1);
        }
    }
}



/*QUAKED trigger_deathpit (.5 .5 .5) ?
Causes fog to fade to black as the player falls into it and eventually kills him.
Also causes rockets and grenades inside it to not explode.

//TODO: Change this so it's just heights specified in the worldspawn (or elsewhere), so we don't run into problems
//    OK, but what about missiles and grenades then? huh?!
//        just check in grenade_touch for depth before exploding.

*/
void() trigger_deathpit =
{
    precache_sound("ambience/windfly.wav");

    InitTrigger ();

    if(!self.height)
        self.height = self.absmax_z;

    if(!self.lip)
        self.lip = self.absmin_z + 160;

        //??????????????????????????????????????
    //dprint_str_f_str("height = ", self.height, "\n");
    //dprint_str_f_str("lip = ", self.lip, "\n");

    self.touch = deathpit_touch;

};






//
// Door crap
//

//Just for utility
void(void() action) door_do =
{
    entity    starte;
    starte = self;
    do
    {
        action ();
        self = self.enemy;
    } while ((self != starte) && (self != world));
};


float DTT_STATE_CLOSED = 0;
float DTT_STATE_ENTERED = 1;
float DTT_STATE_OPENING = 2;
float DTT_STATE_CLOSING = 3;

void() door_temp_trigger_check =
{
    float player_inside;
    entity oself;
    oself = self;


    if(self.state == DTT_STATE_CLOSED)
        return;

    activator = self.enemy;
    player_inside = trigger_touching(activator);

    if(activator.health <= 0) player_inside = FALSE;

    if(player_inside){

        if(self.state == DTT_STATE_ENTERED){
            self.state = DTT_STATE_OPENING;
            self = self.owner;
            if(self.message != "")
            {
                centerprint(activator, self.message);
                sound(activator, CHAN_VOICE, "misc/talk.wav", 1, ATTN_NORM);
            }

            door_do(door_go_up);

        }else if(self.state == DTT_STATE_OPENING){
            if(self.owner.state == STATE_TOP){ // Hit the top
                SUB_Remove(); //Bye bye
                return;
            }
        }else if(self.state == DTT_STATE_CLOSING){
            self.state = DTT_STATE_OPENING;
            self = self.owner;
            door_do(door_go_up);

        }

    }else{ // Player is NOT inside

        if(self.state == DTT_STATE_ENTERED){
            self.state = DTT_STATE_CLOSED;
            return; //how did we even get here?

        }else if(self.state == DTT_STATE_OPENING){
            self.state = DTT_STATE_CLOSING;
            self = self.owner;
            door_do(door_go_down);

        }else if(self.state == DTT_STATE_CLOSING){
            if(self.owner.state == STATE_BOTTOM){ // Hit the bottom
                self.state = DTT_STATE_CLOSED;
                return;
            }

        }
    }
    self = oself;
    self.nextthink = time + 0.1;
};

/*
    spawn_field is modified to set the touch funtion to this instead of the original if the .cnt field is set.
*/
void() door_temp_trigger_touch =
{

    if(self.state != DTT_STATE_CLOSED) //Already started opening it
        return;
    if(other.health <= 0)
        return;

    self.state = DTT_STATE_ENTERED;

    self.think = door_temp_trigger_check;

    self.enemy = other; // Activator
    door_temp_trigger_check();

};


/*QUAKED func_door_temp (0 .5 .8)

"angle"        determines the opening direction
"targetname" if set, no touch field will be spawned and a remote button or trigger field activates the door.
"health"    if set, door must be shot open
"speed"        movement speed (100 default)
"wait"        wait before returning (3 default, -1 = never return)
"lip"        lip remaining at end of move (8 default)
"dmg"        damage to inflict when blocked (2 default)
"sounds"
0)    no sound
1)    stone
2)    base
3)    stone chain
4)    screechy metal
*/

void() func_door_temp =

{

    if(self.sounds == 0)
    {
        precache_sound("misc/null.wav");
        precache_sound("misc/null.wav");
        self.noise1 = "misc/null.wav";
        self.noise2 = "misc/null.wav";
    }
    if(self.sounds == 1)
    {
        precache_sound("doors/drclos4.wav");
        precache_sound("doors/doormv1.wav");
        self.noise1 = "doors/drclos4.wav";
        self.noise2 = "doors/doormv1.wav";
    }
    if(self.sounds == 2)
    {
        precache_sound("doors/hydro1.wav");
        precache_sound("doors/hydro2.wav");
        self.noise2 = "doors/hydro1.wav";
        self.noise1 = "doors/hydro2.wav";
    }
    if(self.sounds == 3)
    {
        precache_sound("doors/stndr1.wav");
        precache_sound("doors/stndr2.wav");
        self.noise2 = "doors/stndr1.wav";
        self.noise1 = "doors/stndr2.wav";
    }
    if(self.sounds == 4)
    {
        precache_sound("doors/ddoor1.wav");
        precache_sound("doors/ddoor2.wav");
        self.noise1 = "doors/ddoor2.wav";
        self.noise2 = "doors/ddoor1.wav";
    }

    if(!self.movedir)
        SetMovedir ();

    self.solid = SOLID_BSP;
    self.movetype = MOVETYPE_PUSH;
    setorigin(self, self.origin);
    setmodel(self, self.model);
    self.classname = "door_temp";
    self.spawnflags = self.spawnflags & DOOR_START_OPEN; //Clear all but this one
    self.spawnflags = self.spawnflags | DOOR_TRIGGER_ON_END; //Add this one

    self.blocked = door_blocked;

    if(!self.speed)
        self.speed = 100;
    self.wait = -1;

    if(!self.lip)
        self.lip = 8;
    if(!self.cnt)
        self.cnt = 5; //How many times faster it closes than opens
    if(!self.dmg)
        self.dmg = 2;

    self.pos1 = self.origin;
    if(!self.pos2)
        self.pos2 = self.pos1 + self.movedir*(fabs(self.movedir*self.size) - self.lip);
    else
        self.pos2 = self.pos1 + self.pos2;

// DOOR_START_OPEN is to allow an entity to be lighted in the closed position
// but spawn in the open position
    if(self.spawnflags & DOOR_START_OPEN)
    {
        setorigin(self, self.pos2);
        self.pos2 = self.pos1;
        self.pos1 = self.origin;
    }

    self.state = STATE_BOTTOM;

// LinkDoors can't be done until all of the doors have been spawned, so
// the sizes can be detected properly.
    self.think = LinkDoors;
    self.nextthink = self.ltime + 0.1;
};



/***********************************************************************/
/* Hub biz *************************************************************/
/* Mostly copied from trigger_changelevel ******************************/
/***********************************************************************/

float LEVEL_1 = 2; //Honey
float LEVEL_2 = 4; //Saint

void() hublevel_touch =
{
    if(other.classname != "player")
        return;

    float noexit;
    noexit = cvar_hget(cvarh_noexit);

    if((noexit == 1) || ((noexit == 2) && (mapname != "start")))
    {
        T_Damage(other, self, self, 50000);
        return;
    }

    if(coop || deathmatch)
    {
        bprint(other.netname);
        bprint(" exited the level\n");
    }

    serverflags = serverflags | self.spawnflags;

    if((serverflags & (LEVEL_1|LEVEL_2)) == (LEVEL_1|LEVEL_2)){
        nextmap = "h_end";
    }else if(serverflags & LEVEL_1){
        nextmap = "h_hub1";
    }else if(serverflags & LEVEL_2){
        nextmap = "h_hub2";
    }

    SUB_UseTargets ();

    if((self.spawnflags & 1) && (deathmatch == 0))
    {    // NO_INTERMISSION
        GotoNextMap();
        return;
    }

    self.touch = SUB_Null;

// we can't move people right now, because touch functions are called
// in the middle of C movement code, so set a think time to do it
    self.think = execute_changelevel;
    self.nextthink = time + 0.1;
};

/*QUAKED trigger_backtohub (0.5 0.5 0.5) LEVEL_1 LEVEL_2
When the player touches this, he gets sent back to the hub.

*/
void() trigger_backtohub =
{
    precache_sound("misc/null.wav"); //Don't ask
    precache_sound("misc/runekey.wav"); //Don't ask

    if(!self.spawnflags)
        objerror ("backtohub without level flag set");

    InitTrigger ();
    self.touch = hublevel_touch;
};



/***********************************************************************/
/* A ghost! Whoa!                                                      */
/* The idea for making the ghosts flicker came from rubicon 2          */
/***********************************************************************/

float(float t, float d) ghost_func =
{
    float at, min, max;

    at = mod(t, d);
    at = at / d;
    min = 0;
    max = 1;

    if(at < 0.5){
        at = at * 2;
        return tween_f(min, max, at);

    }else{
        at = (at - 0.5) * 2;
        return tween_f_sq(max, min, at);
    }
}

entity ghost_boss;

void() ghost_think =
{
    entity starte, e;
    float value, value2;

    starte = e = self.owner;


    do{
        value = sin_interpolate(((time+e.delay)* e.speed) * 10);
        value2 = sin_interpolate(((time+e.delay)* e.speed * 12));

        e.alpha = ((value2 + 1) * 0.2) + 0.2;

        e.velocity_z = (value * 5);

        e = e.oldenemy;
    }while(e != world && e != starte);

    self.nextthink = time + 0.05;
}

void() ghost_think_OLD =
{
    entity starte, e;
    float value, value2, dist;


    starte = e = self.owner;

    float caunt = 0;
    do{
        value = ghost_func(time+e.delay, e.speed);
        value2 = ghost_func(time+e.delay, e.speed * 0.5);

        e.alpha = (value2 * 0.5) + 0.3;

        dist = e.origin_z - e.oldorigin_z;
        if(dist > 15){
            e.dmg = -10;
        }else if(dist < -15){
            e.dmg = 10;
        }

        e.velocity_z = ((value - 0.5) * 50) + e.dmg;

        caunt = caunt + 1;
        e = e.oldenemy;
    } while(e != world && e != starte);

    self.nextthink = time + 0.05;
}

void() ghost_touch =
{
    entity oself;
    if(other.classname != "player")
        return;
    if(self.attack_finished > time)
        return;
    if(self.message == "")
        return;

    oself = self;
    self = other;
    if(!infront_v(oself.dest)){
        self = oself;
        return;
    }
    self = oself;


    self.attack_finished = time + 2;

    centerprint(other, self.message);
    sound(other, CHAN_VOICE, "misc/talk.wav", 1, ATTN_NORM);
}


void() ghost =
{


    if(!ghost_boss.think){
        ghost_boss = spawn();
        ghost_boss.classname = "ghost_manager";
        ghost_boss.think = ghost_think;
        ghost_boss.nextthink = time + 1;

        //Link it...
        ghost_boss.owner = self;
        ghost_boss.enemy = self;
        self.oldenemy = ghost_boss;
    }else{

        //Link it...
        self.oldenemy = ghost_boss.owner;
        ghost_boss.enemy.oldenemy = self;
        ghost_boss.enemy = self;
    }

    if(!self.speed) self.speed = 1 + random() * 3;
    if(!self.delay) self.delay = 1 + random() * self.speed;
    self.dmg = 8;

    self.angles = '0 0 0';
    self.movetype = MOVETYPE_NOCLIP;
    self.solid = SOLID_TRIGGER;
    self.touch = ghost_touch;

    self.takedamage = DAMAGE_NO;
    self.th_die = SUB_Null;
    self.th_pain = SUB_NullPain;

    setorigin(self, self.origin);
    self.oldorigin = self.origin;
    setmodel(self, self.model);

    self.dest = (self.mins + self.maxs) * 0.5;
    self.dest_z = self.dest_z - 15;

    setsize(self, self.mins - '60 60 30', self.maxs + '60 60 30');
}


/*********************************************************************************/
// An undulating pumpy thing
/*********************************************************************************/

void() func_pump_use =
{
    self.velocity = '0 0 0';
    self.think = SUB_Null;
    self.nextthink = -1;
    sound(self, CHAN_VOICE, self.noise3, 0.8, 1.8);
    sound(self, CHAN_WEAPON, "misc/null.wav", 0.8, 1.8);
}

void() func_pump_blocked =
{
    if(time < self.attack_finished) return;

    T_Damage(other, self, self, self.dmg);
    self.attack_finished = time + 0.5;
}

void() func_pump_think =
{
    float sin, dir, t;
    if(!self.search_time){
        self.search_time = self.ltime;
    }
    t = ((self.ltime - self.search_time + self.delay) * self.speed) + 5.6; //Magyck numbre!!
    sin = sin_interpolate(t);

/*    if(!self.attack_state){
        dprint_str_f_str("First sine lookup was using ", t, " and ");
        dprint_str_f_str("that resulted in the value ", sin, ".\n");
        self.attack_state = 1;
    }
*/
    self.velocity = self.pos2 * sin;

    dir = ceil(sin);
    if(dir != self.state){
        self.state = 1 - self.state;
        if(self.state){
            sound(self, CHAN_VOICE, self.noise1, 0.6, 1.8);
        }else{
            sound(self, CHAN_WEAPON, self.noise2, 0.6 , 1.8);
        }
    }

    self.nextthink = self.ltime + 0.05;
}

void() func_pump =
{
    self.angles = '0 0 0';
    self.movetype = MOVETYPE_PUSH;    // so it doesn't get pushed by anything
    self.solid = SOLID_BSP;
    self.think = func_pump_think;
    self.blocked = func_pump_blocked;
    self.use = func_pump_use;
    setmodel(self, self.model);

    if(!self.pos2) self.pos2 = '0 0 20';
    if(!self.speed) self.speed = 12.75;
    if(!self.dmg) self.dmg = 5;

    self.pos2 = self.pos2 * 1.2;

    if(!self.noise1) self.noise1 = "honey/ting1.wav";
    if(!self.noise2) self.noise2 = "honey/ting4.wav";
    if(!self.noise3) self.noise3 = "misc/null.wav";

    precache_sound(self.noise1);
    precache_sound(self.noise2);
    precache_sound(self.noise3);

    self.nextthink = time + 0.2;
}
