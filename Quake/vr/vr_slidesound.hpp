// vr_slidesound.hpp -- flat-screen play's slide sound (his words 2026-11-08: "The slide also needs a sound."; ROUND24.md,
// "Lying body: slide and prone"): a scuff as the slide starts (vr/slide_start.wav), then a scrape (vr/slide_loop.wav,
// looped) that follows the speed along the floor with its volume and pitch, fades as the slide slows and stops when it
// ends; vr_flat_slide_sound its volume (0 silent). Made by Misc/quakevr/make_slide_sounds.py.

#pragma once

namespace qvr::slidesound
{

// The map's load (vr_main.cpp's prepare list): the two sounds.
void prepare();
// flatplay::frame, every frame (`on` false: the mode is not running; the scrape stops).
void frame(bool on);

// The scrape's level now (0..1, before vr_flat_slide_sound), the slides started, and the loudest level this slide (tests).
[[nodiscard]] float level();
[[nodiscard]] int starts();
[[nodiscard]] float peakLevel();

} // namespace qvr::slidesound
