// new ai functions
//
// pmack

// ================================================================
// ai_orbit - used to have an object orbit an entity.
//
// destEnt: the entity to orbit
// radius: how large a radius to orbit at (0 will just track)
// offset: center of the orbit. this is added to dest.origin.
//
// Uses self.orbitPosition to determine current destination.
// ================================================================
void(entity destEnt, float radius, vector offset) ai_orbit =
{
    vector dir;
    float dist;

    if(self.orbitPosition > 3)
        self.orbitPosition = 0;
    else if(self.orbitPosition < 0)
        self.orbitPosition = 0;

    traceline (self.origin, destEnt.origin + offset, TRUE, world);
    if(trace_fraction < 1)
    {
        setorigin(self, destEnt.origin + offset);
        self.orbitPosition = self.orbitPosition + 1;
        return;
    }

    if(self.orbitPosition == 0)
    {
        dir = (destEnt.origin + offset) - self.origin;
        dir_x = dir_x + radius;
    }
    else if(self.orbitPosition == 1)
    {
        dir = (destEnt.origin + offset) - self.origin;
         dir_y = dir_y + radius;
    }
    else if(self.orbitPosition == 2)
    {
        dir = (destEnt.origin + offset) - self.origin;
        dir_x = dir_x - radius;
    }
    else
    {
        dir = (destEnt.origin + offset) - self.origin;
         dir_y = dir_y - radius;
    }

    dist = vlen(dir);

    if(dist < 8)
    {
        self.orbitPosition = self.orbitPosition + 1;
    }
    else if(dist < 50)
    {
        self.velocity = normalize(dir);
        self.velocity = self.velocity * 150;
    }
    else
    {
        self.velocity = normalize(dir);
        self.velocity = self.velocity * 500;
    }
};

// ================================================================
// ai_track - used to have an object chase/track an enemy. the object
//        flies directly at the destEnt's view_ofs point.
//
// destEnt: the entity to orbit
// trackSpeed: the velocity multiplier (speed) of the object
// ================================================================
void(entity destEnt, float trackSpeed) ai_track =
{
    vector    dir;

    dir = destEnt.origin + destEnt.view_ofs;
    dir = normalize(dir - self.origin);
    self.velocity = dir * trackSpeed;
};

// ================================================================
// ViolentDeath
// ================================================================

void(string gibname, float dm) AccelerateGib =
{
    entity    new;

    float        offset1;

    new = spawn();
    new.origin = self.origin;
    setmodel(new, gibname);
    setsize(new, '-8 -8 -8', '8 8 8');

    new.velocity = -1.25 * self.velocity;
    makevectors(new.velocity);
    offset1 = random() * 300 - 150;
    new.velocity = new.velocity + v_right * offset1;
    offset1 = random() * 300 - 150;
    new.velocity = new.velocity + v_up * offset1;


    new.movetype = MOVETYPE_BOUNCE;
    new.solid = SOLID_NOT;
    new.avelocity_x = random()*600;
    new.avelocity_y = random()*600;
    new.avelocity_z = random()*600;
    new.think = SUB_Remove;
    new.ltime = time;
    new.nextthink = time + 10 + random()*10;
    new.frame = 0;
    new.flags = 0;
};

void(float gibCount) ViolentDeath =
{
    while(gibCount > 0)
    {
        AccelerateGib ("progs/gib1.mdl", (-4 * gibCount));
        AccelerateGib ("progs/gib2.mdl", (-6 * gibCount));
        AccelerateGib ("progs/gib3.mdl", (-8 * gibCount));
        gibCount = gibCount - 3;
    }
};

entity(string gibname) StartGib =
{
    entity    new;

    new = spawn();
    new.origin = self.origin;
    setmodel(new, gibname);
    setsize(new, '0 0 0', '0 0 0');

    new.movetype = MOVETYPE_BOUNCE;
    new.solid = SOLID_NOT;
    new.think = SUB_Remove;
    new.ltime = time;
    new.nextthink = time + 10 + random()*10;
    new.frame = 0;
    new.flags = 0;

    return new;
};

