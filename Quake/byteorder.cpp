#include "byteorder.hpp"
#include "sys.hpp"
#include "q_stdinc.hpp"

bool host_bigendian;

short (*BigShort)(short l);
short (*LittleShort)(short l);
int (*BigLong)(int l);
int (*LittleLong)(int l);
float (*BigFloat)(float l);
float (*LittleFloat)(float l);

short ShortSwap(short l)
{
    const byte b1 = l & 255;
    const byte b2 = (l >> 8) & 255;

    return (b1 << 8) + b2;
}

short ShortNoSwap(short l)
{
    return l;
}

int LongSwap(int l)
{
    const byte b1 = l & 255;
    const byte b2 = (l >> 8) & 255;
    const byte b3 = (l >> 16) & 255;
    const byte b4 = (l >> 24) & 255;

    return ((int)b1 << 24) + ((int)b2 << 16) + ((int)b3 << 8) + b4;
}

int LongNoSwap(int l)
{
    return l;
}

float FloatSwap(float f)
{
    union
    {
        float f;
        byte b[4];
    } dat1, dat2;

    dat1.f = f;
    dat2.b[0] = dat1.b[3];
    dat2.b[1] = dat1.b[2];
    dat2.b[2] = dat1.b[1];
    dat2.b[3] = dat1.b[0];
    return dat2.f;
}

float FloatNoSwap(float f)
{
    return f;
}

void ByteOrder_Init()
{
    int i = 0x12345678;
    /*    U N I X */

    /*
    BE_ORDER:  12 34 56 78
           U  N  I  X

    LE_ORDER:  78 56 34 12
           X  I  N  U

    PDP_ORDER: 34 12 78 56
           N  U  X  I
    */
    if(*(char*)&i == 0x12)
    {
        host_bigendian = true;
    }
    else if(*(char*)&i == 0x78)
    {
        host_bigendian = false;
    }
    else
    { /* if ( *(char *)&i == 0x34 ) */
        Sys_Error("Unsupported endianism.");
    }

    if(host_bigendian)
    {
        BigShort = ShortNoSwap;
        LittleShort = ShortSwap;
        BigLong = LongNoSwap;
        LittleLong = LongSwap;
        BigFloat = FloatNoSwap;
        LittleFloat = FloatSwap;
    }
    else /* assumed LITTLE_ENDIAN. */
    {
        BigShort = ShortSwap;
        LittleShort = ShortNoSwap;
        BigLong = LongSwap;
        LittleLong = LongNoSwap;
        BigFloat = FloatSwap;
        LittleFloat = FloatNoSwap;
    }
}
