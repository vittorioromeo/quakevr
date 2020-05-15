#include "fshandle.hpp"

#include <cerrno>

/* The following FS_*() stdio replacements are necessary if one is
 * to perform non-sequential reads on files reopened on pak files
 * because we need the bookkeeping about file start/end positions.
 * Allocating and filling in the fshandle_t structure is the users'
 * responsibility when the file is initially opened. */

size_t FS_fread(void* ptr, size_t size, size_t nmemb, fshandle_t* fh)
{
    long byte_size;
    long bytes_read;
    size_t nmemb_read;

    if(!fh)
    {
        errno = EBADF;
        return 0;
    }
    if(!ptr)
    {
        errno = EFAULT;
        return 0;
    }
    if(!size || !nmemb)
    { /* no error, just zero bytes wanted */
        errno = 0;
        return 0;
    }

    byte_size = nmemb * size;
    if(byte_size > fh->length - fh->pos)
    { /* just read to end */
        byte_size = fh->length - fh->pos;
    }
    bytes_read = fread(ptr, 1, byte_size, fh->file);
    fh->pos += bytes_read;

    /* fread() must return the number of elements read,
     * not the total number of bytes. */
    nmemb_read = bytes_read / size;
    /* even if the last member is only read partially
     * it is counted as a whole in the return value. */
    if(bytes_read % size)
    {
        nmemb_read++;
    }

    return nmemb_read;
}

int FS_fseek(fshandle_t* fh, long offset, int whence)
{
    /* I don't care about 64 bit off_t or fseeko() here.
     * the quake/hexen2 file system is 32 bits, anyway. */
    int ret;

    if(!fh)
    {
        errno = EBADF;
        return -1;
    }

    /* the relative file position shouldn't be smaller
     * than zero or bigger than the filesize. */
    switch(whence)
    {
        case SEEK_SET: break;
        case SEEK_CUR: offset += fh->pos; break;
        case SEEK_END: offset = fh->length + offset; break;
        default: errno = EINVAL; return -1;
    }

    if(offset < 0)
    {
        errno = EINVAL;
        return -1;
    }

    if(offset > fh->length)
    { /* just seek to end */
        offset = fh->length;
    }

    ret = fseek(fh->file, fh->start + offset, SEEK_SET);
    if(ret < 0)
    {
        return ret;
    }

    fh->pos = offset;
    return 0;
}

int FS_fclose(fshandle_t* fh)
{
    if(!fh)
    {
        errno = EBADF;
        return -1;
    }
    return fclose(fh->file);
}

long FS_ftell(fshandle_t* fh)
{
    if(!fh)
    {
        errno = EBADF;
        return -1;
    }
    return fh->pos;
}

void FS_rewind(fshandle_t* fh)
{
    if(!fh)
    {
        return;
    }
    clearerr(fh->file);
    fseek(fh->file, fh->start, SEEK_SET);
    fh->pos = 0;
}

int FS_feof(fshandle_t* fh)
{
    if(!fh)
    {
        errno = EBADF;
        return -1;
    }
    if(fh->pos >= fh->length)
    {
        return -1;
    }
    return 0;
}

int FS_ferror(fshandle_t* fh)
{
    if(!fh)
    {
        errno = EBADF;
        return -1;
    }
    return ferror(fh->file);
}

int FS_fgetc(fshandle_t* fh)
{
    if(!fh)
    {
        errno = EBADF;
        return EOF;
    }
    if(fh->pos >= fh->length)
    {
        return EOF;
    }
    fh->pos += 1;
    return fgetc(fh->file);
}

char* FS_fgets(char* s, int size, fshandle_t* fh)
{
    char* ret;

    if(FS_feof(fh))
    {
        return nullptr;
    }

    if(size > (fh->length - fh->pos) + 1)
    {
        size = (fh->length - fh->pos) + 1;
    }

    ret = fgets(s, size, fh->file);
    fh->pos = ftell(fh->file) - fh->start;

    return ret;
}

long FS_filelength(fshandle_t* fh)
{
    if(!fh)
    {
        errno = EBADF;
        return -1;
    }
    return fh->length;
}
