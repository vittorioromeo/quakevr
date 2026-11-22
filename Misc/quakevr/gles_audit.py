#!/usr/bin/env python3
"""gles_audit: count desktop-GL-only API use (not in OpenGL ES 3.2 core) in the engine and VR module.

Usage: python Misc/quakevr/gles_audit.py <Quake dir> [out.txt]
Prints per-feature totals with the top files (file:first line), for QUEST_PLAN.md's audit.
"""
import os
import re
import sys
from collections import defaultdict

# name -> (regex, ES 3.2 status / replacement)
FEATURES = [
    # functions
    ("glPolygonMode", r"\bglPolygonMode\b", "absent (wireframe debug only; NV_polygon_mode on some)"),
    ("glGetTexImage", r"\bglGetTexImage\b", "absent: FBO + glReadPixels"),
    ("glDrawBuffer (singular)", r"\bglDrawBuffer\s*\(", "absent: glDrawBuffers"),
    ("glReadBuffer", r"\bglReadBuffer\s*\(", "core ES3"),
    ("glClearDepth (double)", r"\bglClearDepth\s*\(", "glClearDepthf"),
    ("glDepthRange (double)", r"\bglDepthRange\s*\(", "glDepthRangef"),
    ("glTexImage1D/GL_TEXTURE_1D", r"\bglTexImage1D\b|\bGL_TEXTURE_1D\b", "absent: 2D texture of height 1"),
    ("glTextureView", r"glTextureView", "absent (OES/EXT_texture_view: not on Adreno)"),
    ("glGetBufferSubData", r"glGetBufferSubData", "absent: glMapBufferRange(READ)"),
    ("glClearTexImage", r"glClearTexImage", "absent (EXT_clear_texture rare): clear via FBO"),
    ("glMapBuffer (non-range)", r"\bMapBuffer(Func)?\s*\(|\bglMapBuffer\s*\(", "OES_mapbuffer: use MapBufferRange"),
    ("MultiDrawElementsIndirect", r"MultiDrawElementsIndirect", "EXT_multi_draw_indirect (check on device) or a loop"),
    ("MultiDrawArrays/Elements", r"\bglMultiDraw(Arrays|Elements)\b", "absent (EXT_multi_draw_arrays)"),
    ("ViewportIndexed", r"ViewportIndexed", "OES_viewport_array (geometry shaders only)"),
    ("ClipControl", r"ClipControl\s*\(|gl_clipcontrol_able", "EXT_clip_control (Adreno: check)"),
    ("Bindless textures", r"gl_bindless_able|GetTextureHandleARB|bindless_texture", "absent: bound textures / arrays"),
    ("BufferStorage persistent", r"gl_buffer_storage_able|GL_MAP_PERSISTENT_BIT", "EXT_buffer_storage (Adreno has it)"),
    ("Multi-bind", r"gl_multi_bind_able|BindTextures(Func)?\s*\(|BindBuffersRange", "absent: loops of single binds"),
    ("Timer queries", r"GL_TIMESTAMP|GL_TIME_ELAPSED|QueryCounter|GetQueryObjecti64v|GetQueryObjectui64v", "EXT_disjoint_timer_query"),
    ("GetQueryObjectiv (signed)", r"GetQueryObjectiv", "only uiv in ES"),
    ("glTexStorage3DMultisample", r"TexStorage3DMultisample", "OES_texture_storage_multisample_2d_array"),
    ("glCopyImageSubData", r"glCopyImageSubData|CopyImageSubData", "core ES 3.2"),
    ("NV shading rate image", r"ShadingRateImage", "absent: XR_FB_foveation / QCOM_texture_foveated"),
    ("OVR multiview", r"FramebufferTextureMultiviewOVR|GL_OVR_multiview", "OVR_multiview2 on Quest (the target path)"),
    ("glBlendFuncSeparate", r"glBlendFuncSeparate", "core ES"),
    ("glFramebufferTexture (layered)", r"\bglFramebufferTexture\s*\(", "core ES 3.2"),
    ("glPointSize / PROGRAM_POINT_SIZE", r"\bglPointSize\b|GL_PROGRAM_POINT_SIZE|GL_POINT_SPRITE", "gl_PointSize in VS only"),
    ("glLineWidth", r"\bglLineWidth\b", "ES max width often 1"),
    # tokens
    ("GL_BGRA/BGR formats", r"\bGL_BGRA?\b|GL_UNSIGNED_INT_8_8_8_8_REV", "EXT_texture_format_BGRA8888 (upload only); swizzle on CPU"),
    ("GL_DEPTH_CLAMP", r"GL_DEPTH_CLAMP", "EXT_depth_clamp (Adreno: check)"),
    ("GL_FRAMEBUFFER_SRGB", r"GL_FRAMEBUFFER_SRGB", "EXT_sRGB_write_control"),
    ("GL_MULTISAMPLE enable", r"GL_MULTISAMPLE\b", "always on in ES"),
    ("GL_TEXTURE_CUBE_MAP_SEAMLESS", r"GL_TEXTURE_CUBE_MAP_SEAMLESS", "always seamless in ES 3"),
    ("Normalized 16-bit formats", r"\bGL_R16\b|\bGL_RG16\b|\bGL_RGB16\b|\bGL_RGBA16\b|GL_R16_SNORM|GL_RGBA16_SNORM", "EXT_texture_norm16"),
    ("Float render targets", r"GL_RGBA16F|GL_RGB16F|GL_R11F_G11F_B10F|GL_RGBA32F|GL_R32F|GL_RG16F|GL_R16F", "EXT_color_buffer_half_float/float (renderable)"),
    ("GL_DEPTH_COMPONENT32 (int)", r"GL_DEPTH_COMPONENT32\b", "absent: DEPTH_COMPONENT32F / 24"),
    ("GL_PRIMITIVE_RESTART (index)", r"GL_PRIMITIVE_RESTART\b|PrimitiveRestartIndex", "PRIMITIVE_RESTART_FIXED_INDEX only"),
    ("GL_CLIP_DISTANCE", r"GL_CLIP_DISTANCE|gl_ClipDistance", "EXT_clip_cull_distance"),
    ("GL_TEXTURE_LOD_BIAS", r"GL_TEXTURE_LOD_BIAS", "absent: bias in shader"),
    ("GL_SAMPLES_PASSED", r"GL_SAMPLES_PASSED", "ANY_SAMPLES_PASSED only"),
    ("GL_QUADS/POLYGON", r"\bGL_QUADS\b|\bGL_POLYGON\b", "absent: triangles"),
    ("GL_DOUBLE", r"\bGL_DOUBLE\b", "absent"),
    ("Compute dispatch", r"DispatchCompute", "core ES 3.1"),
    ("SSBO", r"GL_SHADER_STORAGE_BUFFER", "core ES 3.1 (VS SSBO count: check on device)"),
    ("Image load/store", r"BindImageTexture", "core ES 3.1 (r32ui/rgba8/rgba16f/r32f only, immutable)"),
    ("Geometry shaders", r"GL_GEOMETRY_SHADER", "core ES 3.2 (slow on Adreno)"),
    ("Tessellation", r"GL_TESS_", "core ES 3.2"),
    # GLSL
    ("GLSL #version 4xx literals", r"#version 4\d\d", "#version 320 es + precision header"),
    ("GLSL gl_DrawID/BaseInstance", r"gl_DrawID|gl_BaseInstance|shader_draw_parameters", "absent: per-draw uniform/attrib"),
    ("GLSL gl_Layer/ViewportIndex in VS", r"gl_ViewportIndex|gl_Layer\b", "geometry shader / multiview"),
    ("GLSL noperspective", r"\bnoperspective\b", "NV_shader_noperspective_interpolation"),
    ("GLSL textureQueryLevels/Lod", r"textureQueryLevels|textureQueryLod", "absent: pass as uniform"),
    ("GLSL sampler1D/2DRect", r"sampler1D|sampler2DRect", "absent"),
    ("GLSL uint64/int64", r"uint64_t|int64_t|uvec2.*sampler|GL_ARB_gpu_shader_int64", "absent"),
    ("GLSL #extension GL_ARB", r"#extension GL_ARB|#extension GL_NV|#extension GL_AMD", "desktop extensions"),
]

SKIP_DIRS = ("external", "thirdparty", "doom")
EXTS = (".c", ".cpp", ".h", ".hpp", ".inc")


def main():
    root = sys.argv[1]
    out = open(sys.argv[2], "w") if len(sys.argv) > 2 else sys.stdout
    compiled = [(n, re.compile(rx), note) for n, rx, note in FEATURES]
    hits = defaultdict(lambda: defaultdict(list))
    for d, dirs, files in os.walk(root):
        dirs[:] = [x for x in dirs if x not in SKIP_DIRS]
        for f in files:
            if not f.endswith(EXTS):
                continue
            p = os.path.join(d, f)
            rel = os.path.relpath(p, root).replace("\\", "/")
            with open(p, encoding="utf-8", errors="replace") as fh:
                for i, line in enumerate(fh, 1):
                    for n, rx, _ in compiled:
                        if rx.search(line):
                            hits[n][rel].append(i)
    for n, _, note in compiled:
        files = hits.get(n, {})
        total = sum(len(v) for v in files.values())
        top = sorted(files.items(), key=lambda kv: -len(kv[1]))[:6]
        tops = ", ".join(f"{k}:{v[0]}({len(v)})" for k, v in top)
        print(f"{n} | {total} lines, {len(files)} files | ES: {note} | {tops}", file=out)


if __name__ == "__main__":
    main()
