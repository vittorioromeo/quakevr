#pragma once

// vr_viewuv_glsl.h -- QvrViewUV, for the passes that read a view's targets by a uv over the view (the eyes'
// post-process, the window mirror): under dynamic resolution the eyes' targets are allocated at its ceiling and a frame
// draws their bottom left part (vr_stereo.hpp ViewSize), so a uv over the drawn part (0..1) is scaled to the targets'
// and kept on the drawn part's texels (where clamp-to-edge kept the whole texture's).

// `map` (stereo::viewUvMap): the drawn part over the texture (xy) and half a texel (zw), in its uv. A whole texture's is
// exactly 1 1 (made on the CPU): its uv is then left as it is, the texture's clamp-to-edge doing the rest as before (a
// coordinate clamped to the edge texel's centre is the same texel, but the filter's rounding may differ by a bit).
#define QVR_VIEW_UV_GLSL \
"vec2 QvrViewUV(vec2 uv, vec4 map)\n" \
"{\n" \
"	return all(equal(map.xy, vec2(1.0))) ? uv : clamp(uv * map.xy, map.zw, map.xy - map.zw);\n" \
"}\n"
