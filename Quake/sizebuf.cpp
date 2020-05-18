#include "sizebuf.hpp"
#include "zone.hpp"
#include "common.hpp"
#include "console.hpp"
#include "quakedef.hpp"

void SZ_Alloc(sizebuf_t* buf, int startsize)
{
    if(startsize < 256)
    {
        startsize = 256;
    }

    buf->data = (byte*)Hunk_AllocName(startsize, "sizebuf");
    buf->maxsize = startsize;
    buf->cursize = 0;
}


void SZ_Free(sizebuf_t* buf)
{
    //	Z_Free (buf->data);
    //	buf->data = nullptr;
    //	buf->maxsize = 0;
    buf->cursize = 0;
}

void SZ_Clear(sizebuf_t* buf)
{
    buf->cursize = 0;
}

void* SZ_GetSpace(sizebuf_t* buf, int length)
{
    void* data;

    if(buf->cursize + length > buf->maxsize)
    {
        if(!buf->allowoverflow)
        {
            Host_Error(
                "SZ_GetSpace: overflow without allowoverflow set"); // ericw --
        }

        if(length > buf->maxsize)
        {
            Sys_Error("SZ_GetSpace: %i is > full buffer size", length);
        }

        buf->overflowed = true;
        Con_Printf("SZ_GetSpace: overflow");
        SZ_Clear(buf);
    }

    data = buf->data + buf->cursize;
    buf->cursize += length;

    return data;
}

void SZ_Write(sizebuf_t* buf, const void* data, int length)
{
    Q_memcpy(SZ_GetSpace(buf, length), data, length);
}

void SZ_Print(sizebuf_t* buf, const char* data)
{
    int len = Q_strlen(data) + 1;

    if(buf->data[buf->cursize - 1])
    { /* no trailing 0 */
        Q_memcpy((byte*)SZ_GetSpace(buf, len), data, len);
    }
    else
    { /* write over trailing 0 */
        Q_memcpy((byte*)SZ_GetSpace(buf, len - 1) - 1, data, len);
    }
}
