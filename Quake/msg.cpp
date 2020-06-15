#include "msg.hpp"
#include "q_stdinc.hpp"
#include "sizebuf.hpp"
#include "byteorder.hpp"
#include "common.hpp"
#include "mathlib.hpp"
#include "net.hpp"
#include "protocol.hpp"
#include "sys.hpp"

//
// writing functions
//

void MSG_WriteChar(sizebuf_t* sb, int c)
{
#ifdef PARANOID
    if(c < -128 || c > 127)
    {
        Sys_Error("MSG_WriteChar: range error");
    }
#endif

    byte* const buf = (byte*)SZ_GetSpace(sb, 1);
    buf[0] = c;
}

void MSG_WriteUnsignedChar(sizebuf_t* sb, unsigned char c)
{
#ifdef PARANOID
    if(c < 0 || c > 255)
    {
        Sys_Error("MSG_WriteByte: range error");
    }
#endif

    byte* const buf = (byte*)SZ_GetSpace(sb, 1);
    buf[0] = c;
}

void MSG_WriteByte(sizebuf_t* sb, int c)
{
#ifdef PARANOID
    // TODO VR: (P2) always fires
    // if(c < 0 || c > 255) Sys_Error("MSG_WriteByte: range error");
#endif

    byte* const buf = (byte*)SZ_GetSpace(sb, 1);
    buf[0] = c;
}

void MSG_WriteShort(sizebuf_t* sb, int c)
{
#ifdef PARANOID
    // if(c < -32768 || c > 32767)
    // if (c < 0|| c > 65535)
    {
        // TODO VR: (P2) always fires - seems both signed and unsigned are being
        // passed Sys_Error("MSG_WriteShort: range error");
    }
#endif

    byte* const buf = (byte*)SZ_GetSpace(sb, 2);
    // buf[0] = c & 0b1111'1111'0000'0000;
    buf[0] = c & 0xff;
    buf[1] = c >> 8;
}

void MSG_WriteLong(sizebuf_t* sb, int c)
{
    byte* const buf = (byte*)SZ_GetSpace(sb, 4);
    buf[0] = c & 0xff;
    buf[1] = (c >> 8) & 0xff;
    buf[2] = (c >> 16) & 0xff;
    buf[3] = c >> 24;
}

void MSG_WriteFloat(sizebuf_t* sb, float f)
{
    union
    {
        float f;
        int l;
    } dat;

    dat.f = f;
    dat.l = LittleLong(dat.l);

    SZ_Write(sb, &dat.l, 4);
}

void MSG_WriteString(sizebuf_t* sb, const char* s)
{
    if(!s)
    {
        SZ_Write(sb, "", 1);
    }
    else
    {
        SZ_Write(sb, s, Q_strlen(s) + 1);
    }
}

// johnfitz -- original behavior, 13.3 fixed point coords, max range +-4096
void MSG_WriteCoord16(sizebuf_t* sb, float f)
{
    MSG_WriteShort(sb, Q_rint(f * 8));
}

// johnfitz -- 16.8 fixed point coords, max range +-32768
void MSG_WriteCoord24(sizebuf_t* sb, float f)
{
    MSG_WriteShort(sb, f);
    MSG_WriteByte(sb, (int)(f * 255) % 255);
}

// johnfitz -- 32-bit float coords
void MSG_WriteCoord32f(sizebuf_t* sb, float f)
{
    MSG_WriteFloat(sb, f);
}

void MSG_WriteCoord(sizebuf_t* sb, float f, unsigned int flags)
{
    if(flags & PRFL_FLOATCOORD)
    {
        MSG_WriteFloat(sb, f);
    }
    else if(flags & PRFL_INT32COORD)
    {
        MSG_WriteLong(sb, Q_rint(f * 16));
    }
    else if(flags & PRFL_24BITCOORD)
    {
        MSG_WriteCoord24(sb, f);
    }
    else
    {
        MSG_WriteCoord16(sb, f);
    }
}

void MSG_WriteAngle(sizebuf_t* sb, float f, unsigned int flags)
{
    if(flags & PRFL_FLOATANGLE)
    {
        MSG_WriteFloat(sb, f);
    }
    else if(flags & PRFL_SHORTANGLE)
    {
        MSG_WriteShort(sb, Q_rint(f * 65536.0 / 360.0) & 65535);
    }
    else
    {
        MSG_WriteByte(sb, Q_rint(f * 256.0 / 360.0) & 255);
        // johnfitz -- use Q_rint instead of (int)
    }
}

// johnfitz -- for PROTOCOL_QUAKEVR
void MSG_WriteAngle16(sizebuf_t* sb, float f, unsigned int flags)
{
    if(flags & PRFL_FLOATANGLE)
    {
        MSG_WriteFloat(sb, f);
    }
    else
    {
        MSG_WriteShort(sb, Q_rint(f * 65536.0 / 360.0) & 65535);
    }
}
// johnfitz

void MSG_WriteVec3(sizebuf_t* sb, const qvec3& v, unsigned int flags)
{
    MSG_WriteCoord(sb, v[0], flags);
    MSG_WriteCoord(sb, v[1], flags);
    MSG_WriteCoord(sb, v[2], flags);
}

//
// reading functions
//
int msg_readcount;
bool msg_badread;

void MSG_BeginReading()
{
    msg_readcount = 0;
    msg_badread = false;
}

// returns -1 and sets msg_badread if no more characters are available
[[nodiscard]] int MSG_ReadChar()
{
    if(msg_readcount + 1 > net_message.cursize)
    {
        msg_badread = true;
        return -1;
    }

    const int c = (signed char)net_message.data[msg_readcount];
    msg_readcount++;

    return c;
}

[[nodiscard]] unsigned char MSG_ReadUnsignedChar()
{
    if(msg_readcount + 1 > net_message.cursize)
    {
        msg_badread = true;
        return -1;
    }

    const auto c = (unsigned char)net_message.data[msg_readcount];
    msg_readcount++;

    return c;
}

[[nodiscard]] int MSG_ReadByte()
{
    if(msg_readcount + 1 > net_message.cursize)
    {
        msg_badread = true;
        return -1;
    }

    const int c = (unsigned char)net_message.data[msg_readcount];
    msg_readcount++;

    return c;
}

[[nodiscard]] int MSG_ReadShort()
{
    if(msg_readcount + 2 > net_message.cursize)
    {
        msg_badread = true;
        return -1;
    }

    const int c = (short)(net_message.data[msg_readcount] +
                          (net_message.data[msg_readcount + 1] << 8));

    msg_readcount += 2;

    return c;
}

[[nodiscard]] int MSG_ReadLong()
{
    if(msg_readcount + 4 > net_message.cursize)
    {
        msg_badread = true;
        return -1;
    }

    const int c = net_message.data[msg_readcount] +
                  (net_message.data[msg_readcount + 1] << 8) +
                  (net_message.data[msg_readcount + 2] << 16) +
                  (net_message.data[msg_readcount + 3] << 24);

    msg_readcount += 4;

    return c;
}

[[nodiscard]] float MSG_ReadFloat()
{
    union
    {
        byte b[4];
        float f;
        int l;
    } dat;

    dat.b[0] = net_message.data[msg_readcount];
    dat.b[1] = net_message.data[msg_readcount + 1];
    dat.b[2] = net_message.data[msg_readcount + 2];
    dat.b[3] = net_message.data[msg_readcount + 3];
    msg_readcount += 4;

    dat.l = LittleLong(dat.l);

    return dat.f;
}

[[nodiscard]] const char* MSG_ReadString()
{
    static char string[2048];

    size_t l = 0;
    do
    {
        const int c = MSG_ReadByte();
        if(c == -1 || c == 0)
        {
            break;
        }
        string[l] = c;
        l++;
    } while(l < sizeof(string) - 1);

    string[l] = 0;

    return string;
}

// johnfitz -- original behavior, 13.3 fixed point coords, max range +-4096
[[nodiscard]] float MSG_ReadCoord16()
{
    return MSG_ReadShort() * (1.0 / 8);
}

// johnfitz -- 16.8 fixed point coords, max range +-32768
[[nodiscard]] float MSG_ReadCoord24()
{
    return MSG_ReadShort() + MSG_ReadByte() * (1.0 / 255);
}

// johnfitz -- 32-bit float coords
[[nodiscard]] float MSG_ReadCoord32f()
{
    return MSG_ReadFloat();
}

[[nodiscard]] float MSG_ReadCoord(unsigned int flags)
{
    if(flags & PRFL_FLOATCOORD)
    {
        return MSG_ReadFloat();
    }

    if(flags & PRFL_INT32COORD)
    {
        return MSG_ReadLong() * (1.0 / 16.0);
    }

    if(flags & PRFL_24BITCOORD)
    {
        return MSG_ReadCoord24();
    }

    return MSG_ReadCoord16();
}

[[nodiscard]] float MSG_ReadAngle(unsigned int flags)
{
    if(flags & PRFL_FLOATANGLE)
    {
        return MSG_ReadFloat();
    }

    if(flags & PRFL_SHORTANGLE)
    {
        return MSG_ReadShort() * (360.0 / 65536);
    }

    return MSG_ReadChar() * (360.0 / 256);
}

// johnfitz -- for PROTOCOL_QUAKEVR
[[nodiscard]] float MSG_ReadAngle16(unsigned int flags)
{
    if(flags & PRFL_FLOATANGLE)
    {
        return MSG_ReadFloat(); // make sure
    }

    return MSG_ReadShort() * (360.0 / 65536);
}
// johnfitz

[[nodiscard]] qvec3 MSG_ReadVec3(unsigned int flags)
{
    return {MSG_ReadCoord(flags), MSG_ReadCoord(flags), MSG_ReadCoord(flags)};
}
