/*
 * Ogg/Vorbis streaming music support, loosely based on several open source
 * Quake engine based projects with many modifications.
 *
 * Copyright (C) 2010-2012 O.Sezer <sezero@users.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "quakedef.h"

#if defined(USE_CODEC_VORBIS)
#include "snd_codec.h"
#include "snd_codeci.h"
#include "snd_vorbis.h"
#include "vr/vr_api.h"

/* QVR: the decoders. VORBIS_USE_STB: stb_vorbis alone (a build without
 * libvorbisfile: the Quest's Android build). libvorbisfile (or Tremor) with
 * QVR_STB_VORBIS: stb_vorbis in the fake-Android mode only (-fakeandroid),
 * so the Quest's decoder is tested on the PC (vr_music_decode,
 * Misc/quakevr/questogg_test.sh); Quake/vr/external/stb_vorbis/README.md.
 * The same codec either way: the "ogg" extension, the same music path. */
#if defined(VORBIS_USE_STB)
#define VORBIS_HAVE_LIB 0
#define VORBIS_HAVE_STB 1
#elif defined(QVR_STB_VORBIS)
#define VORBIS_HAVE_LIB 1
#define VORBIS_HAVE_STB 1
#else
#define VORBIS_HAVE_LIB 1
#define VORBIS_HAVE_STB 0
#endif

#if VORBIS_HAVE_LIB
#define OV_EXCLUDE_STATIC_CALLBACKS
#if defined(VORBIS_USE_TREMOR)
/* for Tremor / Vorbisfile api differences,
 * see doc/diff.html in the Tremor package. */
#include <tremor/ivorbisfile.h>
#else
#include <vorbis/vorbisfile.h>
#endif
#endif

#if VORBIS_HAVE_STB
#define STB_VORBIS_HEADER_ONLY
#include "vr/external/stb_vorbis/stb_vorbis.c"
#endif

/* Vorbis codec can return the samples in a number of different
 * formats, we use the standard signed short format. */
#define VORBIS_SAMPLEBITS 16
#define VORBIS_SAMPLEWIDTH 2
#define VORBIS_SIGNED_DATA 1

/* QVR: one stream's decoder state (stream->priv) */
typedef struct
{
#if VORBIS_HAVE_LIB
	OggVorbis_File ov;
#endif
#if VORBIS_HAVE_STB
	stb_vorbis *stb;	/* non-NULL: this stream is stb_vorbis's */
#endif
	int channels;
} vorbis_stream_t;

/* QVR: whether new streams decode with stb_vorbis */
static qboolean S_VORBIS_UseStb (void)
{
#if !VORBIS_HAVE_LIB
	return true;
#elif VORBIS_HAVE_STB
	return VR_Android () != 0;
#else
	return false;
#endif
}

const char *S_VORBIS_DecoderName (void)
{
	if (S_VORBIS_UseStb ())
		return "stb_vorbis";
#if defined(VORBIS_USE_TREMOR)
	return "tremor";
#else
	return "libvorbisfile";
#endif
}

#if VORBIS_HAVE_LIB
/* CALLBACK FUNCTIONS: */

static int ovc_fclose (void *f)
{
	return 0;		/* we fclose() elsewhere. */
}

static int ovc_fseek (void *f, ogg_int64_t off, int whence)
{
	if (f == NULL) return (-1);
	return FS_fseek((fshandle_t *)f, (long) off, whence);
}

static ov_callbacks ovc_qfs =
{
	(size_t (*)(void *, size_t, size_t, void *))	FS_fread,
	(int (*)(void *, ogg_int64_t, int))		ovc_fseek,
	(int (*)(void *))				ovc_fclose,
	(long (*)(void *))				FS_ftell
};

static qboolean S_VORBIS_OpenLib (snd_stream_t *stream, vorbis_stream_t *vs)
{
	OggVorbis_File *ovFile = &vs->ov;
	vorbis_info *ovf_info;
	long numstreams;
	int res;

	res = ov_open_callbacks(&stream->fh, ovFile, NULL, 0, ovc_qfs);
	if (res != 0)
	{
		Con_Printf("%s is not a valid Ogg Vorbis file (error %i).\n",
				stream->name, res);
		return false;
	}

	if (!ov_seekable(ovFile))
	{
		Con_Printf("Stream %s not seekable.\n", stream->name);
		goto _fail;
	}

	ovf_info = ov_info(ovFile, 0);
	if (!ovf_info)
	{
		Con_Printf("Unable to get stream info for %s.\n", stream->name);
		goto _fail;
	}

	/* FIXME: handle section changes */
	numstreams = ov_streams(ovFile);
	if (numstreams != 1)
	{
		Con_Printf("More than one (%ld) stream in %s.\n",
					numstreams, stream->name);
		goto _fail;
	}

	if (ovf_info->channels != 1 && ovf_info->channels != 2)
	{
		Con_Printf("Unsupported number of channels %d in %s\n",
					ovf_info->channels, stream->name);
		goto _fail;
	}

	stream->info.rate = ovf_info->rate;
	vs->channels = ovf_info->channels;
	return true;
_fail:
	ov_clear(ovFile);
	return false;
}

static int S_VORBIS_ReadLib (vorbis_stream_t *vs, int bytes, void *buffer)
{
	int	section;	/* FIXME: handle section changes */
	int	cnt, res, rem;
	char *	ptr;

	cnt = 0; rem = bytes;
	ptr = (char *) buffer;
	while (1)
	{
	/* # ov_read() from libvorbisfile returns the decoded PCM audio
	 *   in requested endianness, signedness and word size.
	 * # ov_read() from Tremor (libvorbisidec) returns decoded audio
	 *   always in host-endian, signed 16 bit PCM format.
	 * # For both of the libraries, if the audio is multichannel,
	 *   the channels are interleaved in the output buffer.
	 */
		res = ov_read( &vs->ov, ptr, rem,
#ifndef VORBIS_USE_TREMOR
				host_bigendian,
				VORBIS_SAMPLEWIDTH,
				VORBIS_SIGNED_DATA,
#endif
				&section );
		if (res <= 0)
			break;
		rem -= res;
		cnt += res;
		if (rem <= 0)
			break;
		ptr += res;
	}

	if (res < 0)
		return res;
	return cnt;
}
#endif	/* VORBIS_HAVE_LIB */

#if VORBIS_HAVE_STB
/* QVR: stb_vorbis reads the stream's part of its file itself (a pak's
 * entry, a rerelease's file in place: fh.start, fh.length) */
static qboolean S_VORBIS_OpenStb (snd_stream_t *stream, vorbis_stream_t *vs)
{
	stb_vorbis_info info;
	int err = 0;

	if (fseek (stream->fh.file, stream->fh.start, SEEK_SET) != 0)
	{
		Con_Printf ("Couldn't seek in %s.\n", stream->name);
		return false;
	}
	vs->stb = stb_vorbis_open_file_section (stream->fh.file, 0, &err, NULL, (unsigned int) stream->fh.length);
	if (!vs->stb)
	{
		Con_Printf ("%s is not a valid Ogg Vorbis file (stb_vorbis error %i).\n", stream->name, err);
		return false;
	}
	info = stb_vorbis_get_info (vs->stb);
	if (info.channels != 1 && info.channels != 2)
	{
		Con_Printf ("Unsupported number of channels %d in %s\n", info.channels, stream->name);
		stb_vorbis_close (vs->stb);
		vs->stb = NULL;
		return false;
	}
	stream->info.rate = (int) info.sample_rate;
	vs->channels = info.channels;
	return true;
}

/* host-endian signed 16-bit, interleaved, as ov_read gives it */
static int S_VORBIS_ReadStb (vorbis_stream_t *vs, int bytes, void *buffer)
{
	const int framebytes = VORBIS_SAMPLEWIDTH * vs->channels;
	int frames = bytes / framebytes, cnt = 0, got;
	short *ptr = (short *) buffer;

	while (frames > 0)
	{
		got = stb_vorbis_get_samples_short_interleaved (vs->stb, vs->channels, ptr, frames * vs->channels);
		if (got <= 0)
			break;
		ptr += got * vs->channels;
		cnt += got * framebytes;
		frames -= got;
	}
	return cnt;
}
#endif	/* VORBIS_HAVE_STB */

static qboolean S_VORBIS_CodecInitialize (void)
{
	return true;
}

static void S_VORBIS_CodecShutdown (void)
{
}

static qboolean S_VORBIS_CodecOpenStream (snd_stream_t *stream)
{
	vorbis_stream_t *vs;
	qboolean ok;

	vs = (vorbis_stream_t *) Z_Malloc(sizeof(vorbis_stream_t));
	stream->priv = vs;
#if VORBIS_HAVE_STB
	if (S_VORBIS_UseStb ())
		ok = S_VORBIS_OpenStb (stream, vs);
	else
#endif
	{
#if VORBIS_HAVE_LIB
		ok = S_VORBIS_OpenLib (stream, vs);
#else
		ok = false;
#endif
	}
	if (!ok)
	{
		Z_Free(vs);
		stream->priv = NULL;
		return false;
	}

	stream->info.channels = vs->channels;
	stream->info.bits = VORBIS_SAMPLEBITS;
	stream->info.width = VORBIS_SAMPLEWIDTH;
	return true;
}

static int S_VORBIS_CodecReadStream (snd_stream_t *stream, int bytes, void *buffer)
{
	vorbis_stream_t *vs = (vorbis_stream_t *) stream->priv;

#if VORBIS_HAVE_STB
	if (vs->stb)
		return S_VORBIS_ReadStb (vs, bytes, buffer);
#endif
#if VORBIS_HAVE_LIB
	return S_VORBIS_ReadLib (vs, bytes, buffer);
#else
	return -1;
#endif
}

static void S_VORBIS_CodecCloseStream (snd_stream_t *stream)
{
	vorbis_stream_t *vs = (vorbis_stream_t *) stream->priv;

#if VORBIS_HAVE_STB
	if (vs->stb)
		stb_vorbis_close (vs->stb);	/* (not the file: S_CodecUtilClose's) */
	else
#endif
	{
#if VORBIS_HAVE_LIB
		ov_clear(&vs->ov);
#endif
	}
	Z_Free(vs);
	S_CodecUtilClose(&stream);
}

static int S_VORBIS_CodecRewindStream (snd_stream_t *stream)
{
	vorbis_stream_t *vs = (vorbis_stream_t *) stream->priv;

#if VORBIS_HAVE_STB
	if (vs->stb)
		return stb_vorbis_seek_start (vs->stb) ? 0 : -1;
#endif
#if VORBIS_HAVE_LIB
/* for libvorbisfile, the ov_time_seek() position argument
 * is seconds as doubles, whereas for Tremor libvorbisidec
 * it is milliseconds as 64 bit integers.
 */
	return ov_time_seek (&vs->ov, 0);
#else
	return -1;
#endif
}

snd_codec_t vorbis_codec =
{
	CODECTYPE_VORBIS,
	true,	/* always available. */
	"ogg",
	S_VORBIS_CodecInitialize,
	S_VORBIS_CodecShutdown,
	S_VORBIS_CodecOpenStream,
	S_VORBIS_CodecReadStream,
	S_VORBIS_CodecRewindStream,
	NULL, /* jump */
	S_VORBIS_CodecCloseStream,
	NULL
};

#endif	/* USE_CODEC_VORBIS */
