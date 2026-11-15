#pragma once

// vr_voicechat.hpp -- proximity voice chat in multiplayer (MP_PLAN.md, B10v).
//
// Sending (a client in a multiplayer game with vr_voicechat on): the microphone (SDL's capture device, the one Voice
// Notes records from: vr_note_device) at 48 kHz mono, RNNoise's noise suppression in 10 ms blocks (vr_voicechat_denoise;
// its voice probability is the voice activation's), Opus (VoIP, vr_voicechat_bitrate, 20 ms frames, in-band FEC) while
// push-to-talk is held (+vr_voicechat, or the off hand's upper button at the mouth when Voice Notes are off) or the voice
// is active (vr_voicechat_mode 1). Each frame (sequence number, a talk spurt's first flag) goes up in the client's next
// move datagram (QVR_CLC_VOICE, unreliable). The host's own player goes the same way (his loopback client).
//
// Relaying (vr_voicerelay.cpp): the server keeps each speaker's last frames and puts the others' into each client's
// datagram (QVR_SVC_VOICE) under his voice budget (vr_net_voice_bytes, at most half his vr_net_rate), oldest first;
// frames that wait too long are dropped.
//
// Receiving: per speaker, a jitter buffer by sequence number (its delay from the measured jitter, set at each spurt's
// start), Opus's FEC for a lost frame whose next came, its packet-loss concealment otherwise; decoded just ahead of the
// mix into a looping sound played at the speaker's head (3D, attenuated by vr_voicechat_attenuation; vr_voicechat_radio:
// full volume anywhere), his volume (vr_voicechat_volume, per player: vr_voicechat_player_volume, vr_voicechat_mute). A
// talking indicator floats over his head while he is heard; yours low in your view while you send.
//
// Single player: nothing (no capture, no sends: cl.maxclients 1). Tests: vr_voicechat_test_tone (a tone in place of the
// microphone), -nomic (no real microphone ever opened: the harness), vr_voicechat_selftest, vr_voicechat_report.

#include "vr_engine.hpp"

struct client_s;

namespace qvr::voicechat
{

inline constexpr int sampleRate = 48000;
inline constexpr int frameSamples = 960;  // 20 ms
inline constexpr int maxFrameBytes = 160; // an Opus frame's bytes at most (64 kbit/s)
inline constexpr int maxFramesPerMessage = 6;
inline constexpr int flagSpurtStart = 1;  // the first frame of a talk spurt
inline constexpr int flagSpurtEnd = 2;    // a talk spurt's end marker (no data)

void init();
void shutdown();
// Once a frame (the client's): capture, encode, play out, the indicators.
void frame();

// CL_SendMove: the frames waiting, after the move (QVR_CLC_VOICE).
void writeClient(sizebuf_t* buf);
// QVR_SVC_VOICE's body (after its sub-command byte).
void parse();

// The off hand's upper button: push-to-talk at the mouth (Voice Notes off, a multiplayer game, push-to-talk mode).
[[nodiscard]] bool offhandButton(bool pressed);

// The microphone being opened or closed on a worker (Voice Notes' meter waits: SDL opens one device at a time).
[[nodiscard]] bool deviceBusy();

// Server (vr_voicerelay.cpp).
// SV_ReadClientMessage's QVR_CLC_VOICE: false a malformed one.
[[nodiscard]] bool serverRead(client_s* client);
// SV_SendClientDatagram: the other players' frames for this client, under his voice budget.
void serverWrite(sizebuf_t* msg, client_s* client);
// A client slot freed or a new map: its frames and counts forgotten.
void serverReset(int slot);
void serverInit();
void serverReport();

} // namespace qvr::voicechat
