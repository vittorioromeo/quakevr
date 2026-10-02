/*
Copyright (C) 1996-2001 Id Software, Inc.
Copyright (C) 2010-2011 O. Sezer <sezero@users.sourceforge.net>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

// snd_mem.c: sound caching

#include "quakedef.h"

/*
================
ResampleSfx
================
*/
static void ResampleSfx (sfx_t *sfx, int inrate, int inwidth, byte *data)
{
	int		outcount;
	int		srcsample;
	float	stepscale;
	int		i;
	int		sample, samplefrac, fracstep;
	int		insamples, inloop;
	sfxcache_t	*sc;

	sc = (sfxcache_t *) Cache_Check (&sfx->cache);
	if (!sc)
		return;

	stepscale = (float)inrate / shm->speed;	// this is usually 0.5, 1, or 2

	insamples = sc->length; // QVR: (the band-limited copy's)
	inloop = sc->loopstart;
	outcount = sc->length / stepscale;
	sc->length = outcount;
	if (sc->loopstart != -1)
		sc->loopstart = sc->loopstart / stepscale;

	sc->speed = shm->speed;
	if (loadas8bit.value)
		sc->width = 1;
	else
		sc->width = inwidth;
	sc->stereo = 0;

// resample / decimate to the current source rate

	if (stepscale == 1 && inwidth == 1 && sc->width == 1)
	{
// fast special case
		for (i = 0; i < outcount; i++)
			((signed char *)sc->data)[i] = (int)( (unsigned char)(data[i]) - 128);
	}
	else
	{
// general case
		srcsample = samplefrac = 0;
		fracstep = stepscale*256;
		for (i = 0; i < outcount; i++)
		{
			if (inwidth == 2)
				sample = LittleShort ( ((short *)data)[srcsample] );
			else
				sample = (int)( (unsigned char)(data[srcsample]) - 128) << 8;
			if (sc->width == 2)
				((short *)sc->data)[i] = sample;
			else
				((signed char *)sc->data)[i] = sample >> 8;
			samplefrac += fracstep;
			srcsample += samplefrac >> 8;
			samplefrac &= 255;
		}
	}

// QVR: and band-limited (vr_snd_fullband): the samples held as above repeat the sound's spectrum above its own
// Nyquist (an 11 kHz sound's images over 5.5 kHz), which Quake's 11 kHz lowpass takes out and a mix without it doesn't
	if (sc->fullband)
		VR_SndBandLimit (data, inwidth, insamples, inloop, (int)(stepscale * 256), (short *)(sc->data + sc->fullband), outcount);
}

//=============================================================================

/*
==============
S_LoadSound
==============
*/
sfxcache_t *S_LoadSound (sfx_t *s)
{
	char	namebuffer[256];
	byte	*data;
	wavinfo_t	info;
	int		len;
	float	stepscale;
	float	ratescale;
	int	fullband;
	sfxcache_t	*sc;

// see if still in memory
	sc = (sfxcache_t *) Cache_Check (&s->cache);
	if (sc)
		return sc;

//	Con_Printf ("S_LoadSound: %x\n", (int)stackbuf);

// load it in
	q_strlcpy(namebuffer, "sound/", sizeof(namebuffer));
	q_strlcat(namebuffer, s->name, sizeof(namebuffer));

//	Con_Printf ("loading %s\n",namebuffer);

	data = COM_LoadMallocFile (namebuffer, NULL);

	// VR: a sound made from one of Quake's when no file has its name (VR_SndDerived: played slower, lower)
	ratescale = 1.f;
	if (!data)
	{
		const char *source = VR_SndDerived (s->name, &ratescale);
		if (source)
		{
			q_strlcpy(namebuffer, "sound/", sizeof(namebuffer));
			q_strlcat(namebuffer, source, sizeof(namebuffer));
			data = COM_LoadMallocFile (namebuffer, NULL);
		}
	}

	if (!data)
	{
		Con_Printf ("Couldn't load %s\n", namebuffer);
		return NULL;
	}

	info = GetWavinfo (s->name, data, com_filesize);
	if (info.channels != 1)
	{
		free (data);
		Con_Printf ("%s is a stereo sample\n",s->name);
		return NULL;
	}

	if (info.width != 1 && info.width != 2)
	{
		free (data);
		Con_Printf("%s is not 8 or 16 bit\n", s->name);
		return NULL;
	}

	if (ratescale != 1.f) // VR: a derived sound's rate (slower: lower and longer)
		info.rate = (int)(info.rate * ratescale + 0.5f);

	stepscale = (float)info.rate / shm->speed;
	len = info.samples / stepscale;

	len = len * info.width * info.channels;

	if (info.samples == 0 || len == 0)
	{
		free (data);
		Con_Printf("%s has zero samples\n", s->name);
		return NULL;
	}

	// QVR: room for the band-limited copy (16-bit, after the held samples; ResampleSfx) when it is resampled
	fullband = info.rate != shm->speed ? (len + 3) & ~3 : 0;
	sc = (sfxcache_t *) Cache_Alloc ( &s->cache, (fullband ? fullband + 2 * (int)(info.samples / stepscale) : len) + sizeof(sfxcache_t), s->name);
	if (!sc)
	{
		free (data);
		return NULL;
	}
	sc->fullband = fullband;

	sc->length = info.samples;
	sc->loopstart = info.loopstart;
	sc->speed = info.rate;
	sc->width = info.width;
	sc->stereo = info.channels;

	ResampleSfx (s, sc->speed, sc->width, data + info.dataofs);

	free (data);

	return sc;
}



/*
===============================================================================

WAV loading

===============================================================================
*/

static byte	*data_p;
static byte	*iff_end;
static byte	*last_chunk;
static byte	*iff_data;
static int	iff_chunk_len;

static short GetLittleShort (void)
{
	short val = 0;
	val = *data_p;
	val = val + (*(data_p+1)<<8);
	data_p += 2;
	return val;
}

static int GetLittleLong (void)
{
	int val = 0;
	val = *data_p;
	val = val + (*(data_p+1)<<8);
	val = val + (*(data_p+2)<<16);
	val = val + (*(data_p+3)<<24);
	data_p += 4;
	return val;
}

static void FindNextChunk (const char *name)
{
	while (1)
	{
	// Need at least 8 bytes for a chunk
		if (last_chunk + 8 >= iff_end)
		{
			data_p = NULL;
			return;
		}

		data_p = last_chunk + 4;
		iff_chunk_len = GetLittleLong();
		if (iff_chunk_len < 0 || iff_chunk_len > iff_end - data_p)
		{
			data_p = NULL;
			Con_DPrintf2("bad \"%s\" chunk length (%d)\n", name, iff_chunk_len);
			return;
		}
		last_chunk = data_p + ((iff_chunk_len + 1) & ~1);
		data_p -= 8;
		if (!strncmp((char *)data_p, name, 4))
			return;
	}
}

static void FindChunk (const char *name)
{
	last_chunk = iff_data;
	FindNextChunk (name);
}

#if 0
static void DumpChunks (void)
{
	char	str[5];

	str[4] = 0;
	data_p = iff_data;
	do
	{
		memcpy (str, data_p, 4);
		data_p += 4;
		iff_chunk_len = GetLittleLong();
		Con_Printf ("0x%x : %s (%d)\n", (int)(data_p - 4), str, iff_chunk_len);
		data_p += (iff_chunk_len + 1) & ~1;
	} while (data_p < iff_end);
}
#endif

/*
============
GetWavinfo
============
*/
wavinfo_t GetWavinfo (const char *name, byte *wav, int wavlength)
{
	wavinfo_t	info;
	int	i;
	int	format;
	int	samples;

	memset (&info, 0, sizeof(info));

	if (!wav)
		return info;

	iff_data = wav;
	iff_end = wav + wavlength;

// find "RIFF" chunk
	FindChunk("RIFF");
	if (!(data_p && !strncmp((char *)data_p + 8, "WAVE", 4)))
	{
		Con_Printf("%s missing RIFF/WAVE chunks\n", name);
		return info;
	}

// get "fmt " chunk
	iff_data = data_p + 12;
#if 0
	DumpChunks ();
#endif

	FindChunk("fmt ");
	if (!data_p)
	{
		Con_Printf("%s is missing fmt chunk\n", name);
		return info;
	}
	data_p += 8;
	format = GetLittleShort();
	if (format != WAV_FORMAT_PCM)
	{
		Con_Printf("%s is not Microsoft PCM format\n", name);
		return info;
	}

	info.channels = GetLittleShort();
	info.rate = GetLittleLong();
	data_p += 4 + 2;
	i = GetLittleShort();
	if (i != 8 && i != 16)
		return info;
	info.width = i / 8;

// get cue chunk
	FindChunk("cue ");
	if (data_p)
	{
		data_p += 32;
		info.loopstart = GetLittleLong();
	//	Con_Printf("loopstart=%d\n", sfx->loopstart);

	// if the next chunk is a LIST chunk, look for a cue length marker
		FindNextChunk ("LIST");
		if (data_p)
		{
			if (!strncmp((char *)data_p + 28, "mark", 4))
			{	// this is not a proper parse, but it works with cooledit...
				data_p += 24;
				i = GetLittleLong();	// samples in loop
				info.samples = info.loopstart + i;
		//		Con_Printf("looped length: %i\n", i);
			}
		}
	}
	else
		info.loopstart = -1;

// find data chunk
	FindChunk("data");
	if (!data_p)
	{
		Con_Printf("%s is missing data chunk\n", name);
		return info;
	}

	data_p += 4;
	samples = GetLittleLong() / info.width;

	if (info.samples)
	{
		if (samples < info.samples)
			Sys_Error ("%s has a bad loop length", name);
	}
	else
		info.samples = samples;

	if (info.loopstart >= info.samples)
	{
		Con_Warning ("%s has loop start >= end\n", name);
		info.loopstart = -1;
		info.samples = samples;
	}

	info.dataofs = data_p - wav;

	return info;
}

