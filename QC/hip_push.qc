/* Pushable walls QuickC program
   By Jim Dose'  9/16/96
   Copyright (c)1996 Hipnotic Interactive, Inc.
   All rights reserved.
   Do not distribute.
*/

void() push_use =
    {
    local vector delta;
    local float x;
    local float y;

//    walkmove (other.angles_y, 16 * frametime);
/*
    if( (other.angles_y >= 315) || (other.angles_y < 45))
        {
        walkmove ( 0, 16 * frametime);
        }
    else if( (other.angles_y >= 45) && (other.angles_y < 135))
        {
        walkmove ( 90, 16 * frametime);
        }
    else if( (other.angles_y >= 135) && (other.angles_y < 225))
        {
        walkmove ( 180, 16 * frametime);
        }
    else if( (other.angles_y >= 225) && (other.angles_y < 315))
        {
        walkmove ( 270, 16 * frametime);
        }
    else
        {
        return;
        }
*/
    makevectors(other.angles);

//    x = fabs( v_forward_x );
//    y = fabs( v_forward_y );
    x = fabs( other.velocity_x );
    y = fabs( other.velocity_y );
    dprint( ftos( x ) );
    dprint( ", " );
    dprint( ftos( y ) );
    if( x > y )
        {
        dprint( " x move\n\n\n\n" );
        if( other.velocity_x > 0 )
            {
            walkmove ( 0, 16 * frametime);
            }
        else
            {
            walkmove( 180, 16 * frametime );
            }
        }
    else
        {
        dprint( " y move\n\n\n\n" );
        if( other.velocity_y > 0 )
            {
            walkmove ( 90, 16 * frametime);
            }
        else
            {
            walkmove( 270, 16 * frametime );
            }
        }

    delta = self.origin - self.oldorigin;
    setorigin (self.owner, self.owner.oldorigin + delta );
    };

/*QUAKED func_pushable (0 .5 .8) ?
Pushable walls.
*/

void() func_pushable =
    {
    local entity new;
    local vector newsize;

    self.mangle = self.angles;
    self.angles = '0 0 0';

    self.classname = "pushablewall";
    self.solid = SOLID_BSP;
    self.movetype = MOVETYPE_PUSH;
    setmodel (self, self.model);
    setorigin( self, self.origin );
    setsize (self, self.mins, self.maxs );
    self.oldorigin = self.origin;

    new = spawn();
    new.owner = self;
    new.mangle = self.mangle;
    new.angles = self.angles;

    new.classname = "pushablewallproxy";
    new.solid = SOLID_BBOX;
    new.movetype = MOVETYPE_STEP;
    new.origin = ( self.mins + self.maxs ) * 0.5 + '0 0 1';
    newsize = ( self.maxs - self.mins ) * 0.5;
   new.mins = '-1 -1 0' - newsize;
   new.maxs = '1 1 -2' + newsize;
    setsize( new, new.mins, new.maxs );
    setorigin( new, new.origin );
    new.oldorigin = new.origin;
     new.touch = push_use;
   };
