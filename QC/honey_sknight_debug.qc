
entity D_SK_CURRENTSPEAR;

void() D_SK_printspear{
    vector p,a;
    p = self.origin - self.owner.origin;
    a = self.angles;


//What we want:

//    }else if(self.cnt == ???){ //index
//        self.pos1 = ???; //position relative to sknight
//        self.pos2 = ???; //angle
//        return;
//

    if(self.cnt)
        dprint("\t}else if(self.cnt == ");
    else
        dprint("\tif(self.cnt == ");
    dprint_float(self.cnt);
    dprint("){\n");
    dprint("\t\tself.pos1 = '");
    dprint_float(p_x);
    dprint(" ");
    dprint_float(p_y);
    dprint(" ");
    dprint_float(p_z);
    dprint("';\n");
    dprint("\t\tself.pos2 = '");
    dprint_float(a_x);
    dprint(" ");
    dprint_float(a_y);
    dprint(" ");
    dprint_float(a_z);
    dprint("';\n");
    dprint("\t\treturn;\n");
    dprint("\n");
}

void() D_SK_printspears{

    entity player;
    player = find(world,classname,"player");
    dprint("All spears:\n");
    dprint("-----------\n");
    sknight_spears_do(D_SK_printspear);
    dprint("\t}\n");
    dprint("-----------\n");
    stuffcmd (player, "condump\n");
}

void() D_SK_selectNext =
{
    entity b;
    vector o;

    if(D_SK_CURRENTSPEAR)
        b = D_SK_CURRENTSPEAR;
    else
        b = world;

    D_SK_CURRENTSPEAR = find(b,classname,"spear");

    if(!D_SK_CURRENTSPEAR){ //Was on last one, get the first one again
        D_SK_CURRENTSPEAR = find(world,classname,"spear");
        dprint("Spears: looped around\n");
    }

    o = D_SK_CURRENTSPEAR.origin;
    //oh hello there
    particle(o,'0 0 2',47,12);

}

entity() D_SK_getCur =
{
    if(!D_SK_CURRENTSPEAR)
        D_SK_selectNext();

    return D_SK_CURRENTSPEAR;
}

void() D_SK_setVPos =
{
    entity player, spear;
    vector pos,ang;

    player = find(world,classname,"player");
    spear = D_SK_getCur();

    pos = player.origin;
    pos_z = pos_z + 14;
    setorigin(spear, pos);

    ang = player.v_angle;
    //ang_x = ang_x - 90;
    ang_x = ang_x * -1; //Why?
    spear.angles = ang;
    spear.avelocity = '0 0 0';
    spear.velocity = '0 0 0';



}

/**********************************************************************************************/
/* DEBUG STUFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF                           */
/**********************************************************************************************/


/**********************************************************************************************/
/* Move all spears in a random manner to the given offset                                     */
/**********************************************************************************************/

void(entity e) D_SK_debugSpline;

void () sknight_randomize_spears_think =
{
    vector a, d;

    a = '360 360 360';
    a_x = (a_x * random()) - 180;
    a_y = (a_y * random()) - 180;
    a_z = (a_z * random()) - 180;

    d = '256 256 256';
    d_x = (d_x * random()) - 128;
    d_y = (d_y * random()) - 128;
    d_z = (d_z * random());

//    dprint("offsetting spear ");
//    dprint(ftos(self.cnt));
//    dprint(" with ");
//    dprint(vtos(d));
//    dprint("\n");

    d = d + self.origin;
    //SUB_CalcMoveRotate(d, SKNIGHT_SPEED, a, SUB_Null);

    SUB_CalcMoveSpline(d, a, 3, -256, '-180 0 0', SUB_Null);
    //D_SK_debugSpline(self);

}

void () sknight_randomize_spears =
{
    sknight_spears_do(sknight_randomize_spears_think);
}

/**********************************************************************************************/
/* Draw a debugging outline of the spline path using sprites                                  */
/**********************************************************************************************/

void(vector o, vector a, string spr) D_SK_spawnSprite =
{
    entity tmp;
    tmp = spawn();
    tmp.classname = "tmpSprite";
    tmp.angles = a;
    tmp.nextthink = time + 1;
    tmp.think = SUB_Remove;
    setorigin(tmp,o);
    setmodel(tmp,spr);

}


void(entity spline) D_SK_debugSpline =
{
    float rem;
    rem = FALSE;
    if(!spline){
        spline = spawn();
        setorigin(spline, '0 192 64');

        spline.s_p0 = '0 192 64';
        spline.s_p1 = '0 0 0';
        spline.s_p2 = '0 0 128';
        spline.s_p3 = '0 -192 64';

        spline.numTicks = 64;

        rem = TRUE;
    }

    entity oself;
    oself = self;

    self = spline;
    while(spline.currentTick < spline.numTicks){

        spline.currentTick = spline.currentTick + 1;
        vector p, a;
        p = InterpolatePoint();
        a = InterpolateAngle();
        D_SK_spawnSprite(p,a,"progs/spike.mdl");
    }

    spline.currentTick = 0;


    D_SK_spawnSprite(self.s_p0,'0 0 0', "progs/s_bubble.spr");
    D_SK_spawnSprite(self.s_p1,'0 0 0', "progs/s_bubble.spr");
    D_SK_spawnSprite(self.s_p2,'0 0 0', "progs/s_bubble.spr");
    D_SK_spawnSprite(self.s_p3,'0 0 0', "progs/s_bubble.spr");


    self = oself;

    if(rem)
        remove(spline);
}


/**********************************************************************************************/
/* IMPULSIVE SHIT                                                                             */
/**********************************************************************************************/

void(float i) D_SK_handleImpulse =
{
    if(i == 33)
        D_SK_printspears ();
    if(i == 34)
        sknight_randomize_spears ();
    if(i == 35)
        sknight_reset_spears(1);
    if(i == 36)
        D_SK_selectNext();
    if(i == 37){
        D_SK_selectNext();
        entity oself; oself = self;
        self = D_SK_CURRENTSPEAR;
        sknight_randomize_spears_think();
        D_SK_debugSpline(D_SK_CURRENTSPEAR);
        D_SK_CURRENTSPEAR.think = SUB_Null;
        D_SK_CURRENTSPEAR.velocity = '0 0 0';
        D_SK_CURRENTSPEAR.avelocity = '0 0 0';
        self = oself;
    }
    if(i == 38){
        D_SK_selectNext();
        D_SK_setVPos();
    }

}

