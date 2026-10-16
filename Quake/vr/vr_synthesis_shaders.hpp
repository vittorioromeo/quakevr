#pragma once

namespace qvr::synthesis::shaders
{
inline constexpr int flowIterations = 2;
inline constexpr const char* fullscreen = R"(#version 430
void main()
{
    vec2 p = vec2(gl_VertexID & 1, gl_VertexID >> 1) * 4.0 - 1.0;
    gl_Position = vec4(p, 0.0, 1.0);
}
)";

inline constexpr const char* depth = R"(#version 430
layout(binding=0) uniform sampler2D Depth;
layout(binding=1) uniform sampler2DMS DepthMS;
layout(location=0) uniform vec2 TargetSize;
layout(location=1) uniform vec4 SourceUV;
layout(location=2) uniform mat4 InverseCurrent;
layout(location=6) uniform mat4 Previous;
layout(location=10) uniform vec3 CurrentOrigin;
layout(location=11) uniform int Reversed;
layout(location=12) uniform int Samples;
layout(location=13) uniform int MaskHidden;
layout(location=0) out vec4 Seed;
void main()
{
    vec2 uv = gl_FragCoord.xy / TargetSize;
    vec2 source = SourceUV.xy + uv * SourceUV.zw;
    float z;
    if(Samples > 1)
    {
        ivec2 size = textureSize(DepthMS);
        ivec2 at = clamp(ivec2(source * size), ivec2(0), size - 1);
        z = texelFetch(DepthMS, at, 0).r;
        for(int s = 1; s < Samples; ++s)
        {
            float v = texelFetch(DepthMS, at, s).r;
            z = Reversed != 0 ? max(z, v) : min(z, v);
        }
    }
    else z = textureLod(Depth, source, 0.0).r;
    gl_FragDepth = z;
    if(MaskHidden != 0 && z == (Reversed != 0 ? 1.0 : 0.0))
    {
        // The lenses' hidden area (vr_visibility_mask) is drawn at the near plane. It is no surface: reprojecting it
        // with the camera's translation gives vectors of many screens, which would also leak into the coarse flow levels.
        Seed = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }
    float clipZ = Reversed != 0 ? z : z * 2.0 - 1.0;
    vec4 world = InverseCurrent * vec4(uv * 2.0 - 1.0, clipZ, 1.0);
    vec4 oldClip;
    bool sky = z == (Reversed != 0 ? 0.0 : 1.0);
    if(sky) oldClip = Previous * vec4(world.xyz - CurrentOrigin * world.w, 0.0);
    else oldClip = Previous * world;
    vec2 oldUV = uv;
    float depthDelta = 0.0; // current minus previous window depth of the same static point (3D vectors only)
    if(!isnan(oldClip.w) && !isinf(oldClip.w) && oldClip.w > 0.000001 &&
        !any(isnan(oldClip.xy)) && !any(isinf(oldClip.xy)))
    {
        vec2 candidate = oldClip.xy / oldClip.w * 0.5 + 0.5;
        if(!any(isnan(candidate)) && !any(isinf(candidate)))
        {
            oldUV = candidate;
            float oldZ = oldClip.z / oldClip.w;
            float oldDepth = Reversed != 0 ? oldZ : oldZ * 0.5 + 0.5;
            if(!sky && !isnan(oldDepth) && !isinf(oldDepth)) depthDelta = z - oldDepth;
        }
    }
    Seed = vec4(oldUV - uv, depthDelta, 1.0);
}
)";

// Pyramidal, camera-seeded Lucas-Kanade estimation of actual image motion.
// Flow is current-to-previous UV, not final EXT current-minus-previous NDC.
inline constexpr const char* flow = R"(#version 430
layout(binding=0) uniform sampler2D Current;
layout(binding=1) uniform sampler2D Previous;
layout(binding=2) uniform sampler2D Seed;
layout(location=0) uniform vec2 TargetSize;
layout(location=1) uniform int Level;
layout(location=2) uniform int SeedLevel;
layout(location=0) out vec4 Result;
float luma(sampler2D image, vec2 uv)
{
    return dot(textureLod(image, uv, float(Level)).rgb, vec3(0.2126, 0.7152, 0.0722));
}
bool finite2(vec2 v)
{
    return !any(isnan(v)) && !any(isinf(v));
}
void main()
{
    vec2 uv = gl_FragCoord.xy / TargetSize;
    vec2 stepUV = 1.0 / TargetSize;
    vec2 flow = textureLod(Seed, uv, float(SeedLevel)).xy;
    if(!finite2(flow)) flow = vec2(0.0);
    float xx = 0.0, xy = 0.0, yy = 0.0;
    vec2 b = vec2(0.0);
    float error = 0.0;
    for(int y = -1; y <= 1; ++y)
    for(int x = -1; x <= 1; ++x)
    {
        vec2 at = uv + vec2(x, y) * stepUV;
        vec2 old = at + flow;
        float residual = luma(Previous, old) - luma(Current, at);
        vec2 grad = vec2(luma(Previous, old + vec2(stepUV.x, 0.0)) - luma(Previous, old - vec2(stepUV.x, 0.0)),
                         luma(Previous, old + vec2(0.0, stepUV.y)) - luma(Previous, old - vec2(0.0, stepUV.y))) * 0.5;
        xx += grad.x * grad.x;
        xy += grad.x * grad.y;
        yy += grad.y * grad.y;
        b += grad * residual;
        error += abs(residual);
    }
    float det = xx * yy - xy * xy;
    if(!isnan(det) && !isinf(det) && det > 0.00000001)
    {
        vec2 update = vec2(yy * b.x - xy * b.y, xx * b.y - xy * b.x) / -det;
        float sizeSquared = dot(update, update);
        if(finite2(update) && !isnan(sizeSquared) && !isinf(sizeSquared))
        {
            if(sizeSquared > 4.0) update *= 2.0 * inversesqrt(sizeSquared);
            vec2 candidate = flow + update * stepUV;
            if(finite2(candidate)) flow = candidate;
        }
    }
    if(!finite2(flow)) flow = vec2(0.0);
    float meanError = error / 9.0;
    if(isnan(meanError) || isinf(meanError)) meanError = 0.0;
    Result = vec4(flow, meanError, 1.0);
}
)";

inline constexpr const char* exportImages = R"(#version 430
layout(binding=0) uniform sampler2D Flow;
layout(binding=1) uniform sampler2D Depth;
layout(binding=2) uniform sampler2D CameraSeed;
layout(location=0) uniform vec2 TargetSize;
layout(location=3) uniform vec2 MotionScale;
layout(location=4) uniform vec2 MotionOffset;
layout(location=5) uniform int EncodeSrgb8;
layout(location=6) uniform int FlipY;
layout(location=7) uniform int Use3D;
layout(location=8) uniform int Reversed;
layout(location=9) uniform int MaskHidden;
layout(location=0) out vec4 Motion;
bool finite2(vec2 v)
{
    return !any(isnan(v)) && !any(isinf(v));
}
vec2 toNdcMotion(vec2 flow)
{
    return vec2(-2.0 * flow.x, (FlipY != 0 ? -2.0 : 2.0) * flow.y);
}
bool representableMotion(vec2 motion)
{
    // A finite float32 shader result can still overflow RGBA16F on store.
    return finite2(motion) && all(lessThanEqual(abs(motion), vec2(65504.0)));
}
bool inSrgbRange(vec2 motion)
{
    vec2 encoded = (motion - MotionOffset) / MotionScale;
    return finite2(encoded) && all(greaterThanEqual(encoded, vec2(0.005))) &&
        all(lessThanEqual(encoded, vec2(0.995)));
}
vec2 linearToSrgb(vec2 linearValue)
{
    bvec2 low = lessThanEqual(linearValue, vec2(0.0031308));
    vec2 lower = linearValue * 12.92;
    vec2 upper = 1.055 * pow(linearValue, vec2(1.0 / 2.4)) - 0.055;
    return mix(upper, lower, low);
}
void main()
{
    vec2 uv = gl_FragCoord.xy / TargetSize;
    float sourceDepth = textureLod(Depth, uv, 0.0).r;
    // Hidden area (vr_visibility_mask): drawn at the near plane, so it would pose as the nearest surface of the frame.
    bool hidden = MaskHidden != 0 && sourceDepth == (Reversed != 0 ? 1.0 : 0.0);
    vec4 flowSample = textureLod(Flow, uv, 0.0);
    vec2 flow = flowSample.xy;
    // The flow pass writes alpha 1 and is cleared to 0 first: a texel it never covered (some drivers leave raster gaps)
    // is no estimate, whatever else it holds. The negated test also rejects NaN.
    bool covered = flowSample.w > 0.5;
    // Flow is previousUV-currentUV. EXT uses currentNDC-previousNDC in Vulkan
    // NDC (positive Y down), while this OpenGL producer's UV has positive Y up.
    vec2 motion = toNdcMotion(flow);
    bool fallback = !covered || !finite2(flow) || !representableMotion(motion);
    if(EncodeSrgb8 != 0 && !fallback) fallback = !inSrgbRange(motion);
    if(fallback)
    {
        // A bounded, finite image-space estimate is preferable to a clamped
        // vector: keep camera reprojection when it fits, otherwise declare
        // zero motion for this pixel. Alpha is ignored by EXT and marks the
        // fallback in RGBA diagnostics where the format has alpha.
        vec2 seed = textureLod(CameraSeed, uv, 0.0).xy;
        motion = toNdcMotion(seed);
        if(!finite2(seed) || !representableMotion(motion) ||
            (EncodeSrgb8 != 0 && !inSrgbRange(motion))) motion = vec2(0.0);
    }
    if(hidden)
    {
        motion = vec2(0.0);
        fallback = true; // alpha 0: not estimated
    }
    if(EncodeSrgb8 != 0)
    {
        vec2 linearSample = (motion - MotionOffset) / MotionScale;
        Motion = vec4(linearToSrgb(linearSample), 0.0, 1.0);
    }
    else
    {
        float depthDelta = Use3D != 0 ? textureLod(CameraSeed, uv, 0.0).z : 0.0;
        if(isnan(depthDelta) || isinf(depthDelta)) depthDelta = 0.0;
        Motion = vec4(motion, depthDelta, fallback ? 0.0 : 1.0);
    }
    if(EncodeSrgb8 != 0) Motion.a = fallback ? 0.0 : 1.0;
    gl_FragDepth = hidden ? (Reversed != 0 ? 0.0 : 1.0) : sourceDepth;
}
)";
} // namespace qvr::synthesis::shaders
