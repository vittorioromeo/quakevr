/**************************************************************************/
/* Fog Controlling stuff                                                  */
/**************************************************************************/

void setFog(entity client, float density, vector color) =
{
    stuffcmd(client, "\nfog ");
    stuffcmd_float(client, density);
    stuffcmd(client, " ");
    stuffcmd_float(client, color_x);
    stuffcmd(client, " ");
    stuffcmd_float(client, color_y);
    stuffcmd(client, " ");
    stuffcmd_float(client, color_z);
    stuffcmd(client, "\n");

}

.vector fog_current_color;
.float  fog_current_density;

vector fog_global_color;
float  fog_global_density;

.entity fog_local_owner;
.vector fog_local_color;
.float  fog_local_density;
.vector fog_local_colorCurrent;
.float  fog_local_densityCurrent;
.vector fog_local_colorBase;
.float  fog_local_densityBase;
.float  fog_local_fraction;
.float  fog_local_fractionStep;


.entity fog_override_owner;
.vector fog_override_color;
.float  fog_override_density;
.vector fog_override_colorBase;
.float  fog_override_densityBase;
.float  fog_override_fraction;
.float  fog_override_fractionStep;
.float  fog_override_fractionControl;

float  FOG_OVERRIDE_DIRECT_CONTROL = -99999;

void() fog_updateAll =
{
    if(!VR_GameUtil_EnableHoneyExtensions() && !foundHoneyFogSettings)
    {
        return;
    }

    entity c = self;

    // 1: This is the starting point
    c.fog_current_color = fog_global_color;
    c.fog_current_density = fog_global_density;

    // 2: Figure out the local fog, if any
    c.fog_local_fraction = c.fog_local_fraction + c.fog_local_fractionStep;
    c.fog_local_fraction = clamp(c.fog_local_fraction, 0, 1);

    if(c.fog_local_fraction >= 1){ //Local fog is in full effect
        c.fog_current_color = c.fog_local_color;
        c.fog_current_density = c.fog_local_density;

        //Reset these
        c.fog_local_colorBase = fog_global_color;
        c.fog_local_densityBase = fog_global_density;


    }else if(c.fog_local_fraction <= 0){ //No local fog at all
        SUB_Null();

    }else{ //Local fog is transitioning

        c.fog_current_density = tween_f(c.fog_local_densityBase, c.fog_local_density, c.fog_local_fraction);
        c.fog_current_color = tween_v(c.fog_local_colorBase, c.fog_local_color, c.fog_local_fraction);
    }


    c.fog_local_colorCurrent = c.fog_current_color;
    c.fog_local_densityCurrent = c.fog_current_density;

    // 3: Figure out the override fog if any
    if(c.fog_override_fractionStep != FOG_OVERRIDE_DIRECT_CONTROL){
        // 3.a: Tween the fog as usual

        c.fog_override_fraction = c.fog_override_fraction + c.fog_override_fractionStep;
        c.fog_override_fraction = clamp(c.fog_override_fraction, 0, 1);

        if(c.fog_override_fraction >= 1){ //override fog is in full effect
            c.fog_current_color = c.fog_override_color;
            c.fog_current_density = c.fog_override_density;

            //Reset these
            c.fog_override_colorBase = c.fog_local_colorCurrent;
            c.fog_override_densityBase = c.fog_local_densityCurrent;

        }else if(c.fog_override_fraction <= 0){
            SUB_Null();

        }else{ //override fog is transitioning

            c.fog_current_color = tween_v(c.fog_override_colorBase, c.fog_override_color, c.fog_override_fraction);
            c.fog_current_density = tween_f(c.fog_override_densityBase, c.fog_override_density, c.fog_override_fraction);

        }
    }else{

        // 3.b: The override fog is being controlled directly by an entity
        c.fog_current_color = tween_v(c.fog_override_colorBase, c.fog_override_color, c.fog_override_fractionControl);
        c.fog_current_density = tween_f(c.fog_override_densityBase, c.fog_override_density, c.fog_override_fractionControl);

    }

    // 4: Push the fog
    setFog(c, c.fog_current_density, c.fog_current_color);
}

void(entity c, vector color, float density, float duration) fog_setLocal =
{
    if(self == c.fog_local_owner) return;

    c.fog_local_owner = self;

    if(!(c.fog_local_color == color && c.fog_local_density == density)){

        c.fog_local_color = color;
        c.fog_local_density = density;

        c.fog_local_colorBase = c.fog_local_colorCurrent;
        c.fog_local_densityBase = c.fog_local_densityCurrent;

        c.fog_local_fraction = 0;
    }
    c.fog_local_fractionStep = 0.05 / duration; //FIXME: This magic number should actually be tied to the framerate somehow
    if(c.fog_local_fractionStep <= 0) c.fog_local_fractionStep = 0.05;

}

void(entity c) fog_releaseLocal =
{
    if(self != c.fog_local_owner) return;
    c.fog_local_owner = world;

    if(c.fog_local_fractionStep > 0) c.fog_local_fractionStep = c.fog_local_fractionStep * -1;
}

void fog_setOverride (entity c, vector color, float density, float startAt, float duration) =
{
    c.fog_override_owner = self;

    c.fog_override_color = color;
    c.fog_override_density = density;

    c.fog_override_colorBase = c.fog_current_color;
    c.fog_override_densityBase = c.fog_current_density;

    if(startAt != -1)
        c.fog_override_fraction = startAt;
    else
        c.fog_override_fraction = 0;

    if(duration){
        c.fog_override_fractionStep = 0.05 / duration; //FIXME: This magic number should actually be tied to the framerate somehow
        if(c.fog_override_fractionStep <= 0) c.fog_override_fractionStep = 0.05;

    }else{
        c.fog_override_fractionStep = 0.05;
    }

}

void(entity c) fog_releaseOverride =
{
    if(self != c.fog_override_owner) {
        return;
    }
    c.fog_override_owner = world;

    if(c.fog_override_fractionStep == FOG_OVERRIDE_DIRECT_CONTROL) c.fog_override_fractionStep = -0.05;
    else if(c.fog_override_fractionStep > 0) c.fog_override_fractionStep = c.fog_override_fractionStep * -1;
}

void fog_setOverrideConditional(entity c, vector color, float density, float currentFraction) =
{

    if(c.fog_override_owner == self) return;
    c.fog_override_owner = self;


    c.fog_override_color = color;
    c.fog_override_density = density;

    c.fog_override_colorBase = c.fog_current_color;
    c.fog_override_densityBase = c.fog_current_density;

    c.fog_override_fraction = currentFraction;
    c.fog_override_fractionStep = FOG_OVERRIDE_DIRECT_CONTROL;
    c.fog_override_fractionControl = currentFraction;

}

void fog_setOverrideFraction(entity c, float fraction) =
{
    if(self != c.fog_override_owner) return;
    c.fog_override_fractionControl = clamp(fraction, 0, 1);
}


/***************************************************************************************************/
/* Controlling entities                                                                            */
/***************************************************************************************************/

float foggroup_cnt;
float foggroup_current;

void() fogcontroller_linkgroups =
{
    entity    t, starte;

    self.think = SUB_Null;
    if(self.oldenemy)
        return;        // already linked by another controller


    foggroup_cnt = foggroup_cnt + 1;
    self.cnt = foggroup_cnt;

    if(self.targetname == "")
    {
        self.owner = self;
        return;        // not in a group
    }

    starte = self;
    t = self;

    do
    {
        self.owner = starte;            // master

        if(self.view_ofs) starte.view_ofs = self.view_ofs; //Target Color
        if(self.ideal_yaw) starte.ideal_yaw = self.ideal_yaw; //Target Density
        if(self.attack_finished) starte.attack_finished = self.attack_finished; //Duration

        t = find(t, targetname, self.targetname);
        if(!t)
        {
            self.oldenemy = starte;        // make the chain a loop
            return;
        }
        if(t.classname == "trigger_fogcontroller"){
            if(t.oldenemy)
                objerror ("cross connected fogcontrollers");

            self.oldenemy = t;
            self = t;
        }
    } while (1);
}


float (entity o) trigger_touching =
{
    vector o_maxs, o_mins;
    o_maxs = o.maxs + o.origin;
    o_mins = o.mins + o.origin;

    if(self.mins_x > o_maxs_x)
        return FALSE;
    if(self.mins_y > o_maxs_y)
        return FALSE;
    if(self.mins_z > o_maxs_z)
        return FALSE;
    if(self.maxs_x < o_mins_x)
        return FALSE;
    if(self.maxs_y < o_mins_y)
        return FALSE;
    if(self.maxs_z < o_mins_z)
        return FALSE;
    return TRUE;
};

float (entity o) trigger_touching_group =
{
    entity starte, oself;
    float touching;
    touching = FALSE;

    if(!self.owner) return trigger_touching(o); // Not in a group

    oself = self;
    starte = self.owner;
    self = starte;
    do{
        touching = trigger_touching(o);
        if(touching) break;

        self = self.oldenemy;
    }while(self != starte && self != world);

    self = oself;
    return touching;
}


void() fogcontroller_think =
{
    if(trigger_touching_group(self.enemy)){
        self.nextthink = time + 0.1;
        return;
    }else{
        fog_releaseLocal(self.enemy);
        self.state = 0;
        foggroup_current = 0;
    }
};

void() fogcontroller_enable =
{
    self.state = 1;
    fog_setLocal(self.enemy, self.view_ofs, self.ideal_yaw, self.attack_finished);

    self.think = fogcontroller_think;
    self.nextthink = time + 0.1;
};


void() fogcontroller_touch =
{
    if(other.classname != "player") return;
    if(self.owner) self = self.owner; //Delegate to master




    if(self.state && foggroup_current == self.cnt) return;

    foggroup_current = self.cnt;

    self.enemy = other;
    fogcontroller_enable();

};



/*QUAKED trigger_fogcontroller (.5 .5 .5) ?
    while inside the global fog is faded to the specified color and density.
    the transition takes as long as specified.
    DO NOT make it possible for the player to get from one controller
    to another while the effect from the first is still active.

*/
void() trigger_fogcontroller =
{
    if(!self.view_ofs) self.view_ofs = '1 0 0'; //Target Color
    if(!self.ideal_yaw) self.ideal_yaw = 0.5; //Target Density
    if(!self.attack_finished) self.attack_finished = 1; //Duration


    self.touch = fogcontroller_touch;
    self.think = fogcontroller_linkgroups;
    self.nextthink = time + 0.1;

    InitTrigger();
};
