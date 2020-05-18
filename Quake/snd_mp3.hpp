/* MP3 decoding support using libmad or libmpg123. */

#pragma once

#if defined(USE_CODEC_MP3)

extern snd_codec_t mp3_codec;
int mp3_skiptags(snd_stream_t*);

#endif /* USE_CODEC_MP3 */
