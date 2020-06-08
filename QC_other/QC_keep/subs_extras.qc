/*
============
SUB_RandomRange

Return a random number between min & max (or min if max is 0)
Just make sure max is greater than min...
============
*/
float(float rmin, float rmax) SUB_RandomRange =
{
    if (!rmax)
        return rmin;
    else
        return rmin + random()*(rmax-rmin);
};