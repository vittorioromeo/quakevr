/*
Copyright (C) 2020-2026 Vittorio Romeo and Quake VR contributors

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

// vr_tonemap.h -- the eyes' tone curve, colour grade and dither (vr_tonemap.cpp; docs/vr-port/ROUND17.md): the GLSL
// shared by Ironwail's post-process (gl_shaders.h) and the window's mirror (vr_stereo.cpp), and the renderer's hooks.
// C and C++.
//
// With vr_tonemap the eyes' scene is a float target (RGBA16F instead of Ironwail's RGB10_A2) and the world and
// models write colours above 1 (SceneTone.x in the frame data; Quake's clamp is 1): overbright lightmaps, uncapped
// coloured dynamic lights, lava's glow, additive effects piling up. The post-process adds the glow, then rolls the
// brightest off with the curve below instead of clipping each channel at 1 (which turned orange light yellow and
// white and flattened lit textures), grades, applies the headset's gamma, and dithers last into the 8-bit image.

#ifndef QVR_VR_TONEMAP_H
#define QVR_VR_TONEMAP_H

// QvrTonemap: Quake's gamma-encoded colours as they are shown (a lamp 1; in the float scene, lit walls up to about 4).
// Below the knee (kw.x) nothing changes: the tuned look (vr_light_contrast, bloom) stays. Above it the brightest
// channel is rolled off by an extended Reinhard shoulder (slope 1 at the knee) that reaches 1 at the white point
// (kw.y); the colour is scaled with it, keeping its hue and saturation (an orange light stays orange instead of
// clipping to yellow), and past 1 it turns whiter with how much brighter than 1 it was (0.8 of the way to white at
// the white point), so lava's hottest parts and a flash's core still look hot, yellow-white. QvrGrade: the colour looked up in a 3D table (vr_grade), blended in by strength.
#define QVR_TONE_GLSL \
"vec3 QvrTonemap(vec3 c, vec2 kw)\n" \
"{\n" \
"	float m = max(c.r, max(c.g, c.b));\n" \
"	if (m <= kw.x)\n" \
"		return c;\n" \
"	float t = (m - kw.x) / (1.0 - kw.x);\n" \
"	float w = (kw.y - kw.x) / (1.0 - kw.x);\n" \
"	float top = kw.x + (1.0 - kw.x) * min(t * (1.0 + t / (w * w)) / (1.0 + t), 1.0);\n" \
"	float over = clamp((m - 1.0) / (kw.y - 1.0), 0.0, 1.0);\n" \
"	return mix(c * (top / m), vec3(top), over * 0.8);\n" \
"}\n" \
"vec3 QvrGrade(sampler3D lut, vec3 c, float strength)\n" \
"{\n" \
"	vec3 n = vec3(textureSize(lut, 0));\n" \
"	return mix(c, texture(lut, clamp(c, 0.0, 1.0) * ((n - 1.0) / n) + 0.5 / n).rgb, strength);\n" \
"}\n"

#ifdef __cplusplus
extern "C" {
#endif

unsigned VR_SceneColorFormat (unsigned format);	// GL_CreateFrameBuffers: the scene's colour format (the eyes' float one with vr_tonemap)
float VR_SceneTone (void);						// R_SetupView: the brightest the world and models write (1: Quake's clamp)
float VR_SceneDither (float dither);			// R_SetupView: the scene's screen dither (0 in the eyes with vr_dither: the post-process dithers last)
void VR_PostProcessTone (void);					// GL_PostProcess, the non-palettized program in use: the tone curve, grade (unit 3) and dither

#ifdef __cplusplus
}
#endif

#endif // QVR_VR_TONEMAP_H
