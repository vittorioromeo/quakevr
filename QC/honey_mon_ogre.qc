void() ogre_start =
{
    entity n;

    if(self.spawnflags & SPAWNFLAG_TRIGGER_SPAWN_MULTIPLE && self.cnt != 0)
    {
        self.cnt = self.cnt - 1;
        n = spawn();
        setorigin(n, self.origin);
        n.angles = self.angles;
        n.classname = self.classname;
        n.spawnflags = self.spawnflags;
        n.target = self.target;
        n.killtarget = self.killtarget;
        n.wad = self.wad;

        n.health = 200;

        n.th_stand = ogre_stand1;
        n.th_walk = ogre_walk1;
        n.th_run = ogre_run1;
        n.th_die = ogre_die;
        n.th_melee = ogre_melee;
        n.th_missile = ogre_nail1;
        n.th_pain = ogre_pain;
        if(self.cnt <= -1){
            total_monsters = total_monsters + 1;
            WriteByte(MSG_ALL, SVC_UPDATESTAT);
            WriteByte(MSG_ALL, 12);
            WriteLong (MSG_ALL, total_monsters);
        }
    }else{
        n = self;
    }


    n.solid = SOLID_SLIDEBOX;
    n.movetype = MOVETYPE_STEP;

    setmodel(n, n.wad);
    setsize(n, VEC_HULL2_MIN, VEC_HULL2_MAX);

    self = n;
    ogre_spawncrown();
    teleport_glitter();
    walkmonster_start();
};

// Honey's marksman ogre (its model progs/mogre.mdl, its crown, its far sight and lobbed grenades, its chainsaw's
// extra damage to ogres): only one spawned in a Honey map with Honey's data there. Its .wad (saved; copied by
// ogre_start) tells it from the other marksman.
float(entity e) VR_IsHoneyMarksman = { return e.classname == "monster_ogre_marksman" && e.wad == "progs/mogre.mdl"; };

void() monster_ogre_marksman =
{
    // Anywhere else (Dimension of the Machine, the firing range's dispenser, id's maps, a third-party map such as Down
    // the Gutter, plaw01, or Honey not installed): Dimension of the Machine's marksman, as id's ogre.qc has it: id's
    // ogre under the marksman's classname (the ogre's model and behaviour: ai.qc CheckAnyAttack, OgreCheckAttack).
    // Precaching Honey's model without it there would be a Host_Error.
    float honeyMap = !MG_WorldCampaign() && (cvar("vr_honey_context") || VR_GameUtil_InHoneyMap());
    if(!honeyMap || !VR_Has("progs/mogre.mdl") || !VR_Has("progs/s_light2.spr"))
    {
        self.wad = "progs/ogre.mdl";
        monster_ogre_real();
        return;
    }
    self.wad = "progs/mogre.mdl";
    precache_model("progs/s_light2.spr");
    monster_ogre_real();
};


/*
    MARKSMAN CROWN
*/
vector ogre_crownheight = '0 0 52';

void() ogre_crown_update =
{
    vector ogr_org, ogr_ang, offs;
    float tm, overshoot;

    if(self.owner.enemy){     tm = 3;        offs = '4 4 32';    overshoot = 40;}
    else                {    tm = 8;        offs = '32 32 16';    overshoot = 8;}

    ogr_ang = '0 0 0';
    ogr_ang_x = (4 * random()) - 2;
    if(self.owner.enemy)    ogr_ang_y = self.owner.angles_y + ((40 * random()) - 20);
    else                    ogr_ang_y = self.angles_y + ((20 * random()) - 10) + 180;

    ogr_org = self.owner.origin + ogre_crownheight;
    ogr_org_x = ogr_org_x + (random() * offs_x) - (offs_x / 2);
    ogr_org_y = ogr_org_y + (random() * offs_y) - (offs_y / 2);
    ogr_org_z = ogr_org_z + (random() * offs_z);


    //SUB_CalcMoveSpline_norotate(vector tdest, vector tangle, float ttime, float cpOffset, void() func);
    SUB_CalcMoveSpline_norotate(ogr_org, ogr_ang, tm, overshoot, ogre_crown_update);
};

void() ogre_spawncrown =
{
    entity tf;
    if(!VR_IsHoneyMarksman(self)) return;
    if(self.trigger_field) return;

    tf = spawn();
    tf.owner = self;
    tf.alpha = 0.6;
    tf.movetype = MOVETYPE_NOCLIP;
    setmodel(tf,"progs/s_light2.spr");
    setorigin(tf, self.origin);
    tf.oldorigin = self.angles;


    tf.nextthink = time + 0.2;
    tf.think = ogre_crown_update;
    self.trigger_field = tf;
}

void() ogre_crown_die =
{
    entity tf;
    tf = self.trigger_field;

    if(tf == world)
    {
        return;
    }

    WriteByte(MSG_BROADCAST, SVC_TEMPENTITY);
    WriteByte(MSG_BROADCAST, TE_TAREXPLOSION);
    WriteVec3(MSG_BROADCAST, tf.origin);

    self.trigger_field = world;

    remove(tf);
}
