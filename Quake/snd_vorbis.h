/* Ogg/Vorbis streaming music support. */

#if !defined(_SND_VORBIS_H_)
#define _SND_VORBIS_H_ 1

#if defined(USE_CODEC_VORBIS)

extern snd_codec_t vorbis_codec;
const char *S_VORBIS_DecoderName (void);	/* QVR: "stb_vorbis" (the Quest's, -fakeandroid's), "libvorbisfile" or "tremor" */

#endif	/* USE_CODEC_VORBIS */

#endif	/* ! _SND_VORBIS_H_ */

