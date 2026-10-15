// Pure projection/reprojection math for a future internal frame-synthesis fixture.
// No history, GL resources, runtime extension activation, or composition submission.
#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <cmath>

namespace qvr::synthesis
{

// Map runtime metres to rendered Quake world coordinates. Real tracked motion
// cancels; changes in this mapping are application locomotion/calibration.
[[nodiscard]] inline glm::mat4 worldFromStage(const glm::mat4& quakeView,
    const glm::mat4& stageFromEye, float unitsPerMetre)
{
    glm::mat4 axes{1.f};
    axes[0] = glm::vec4{0.f, -1.f, 0.f, 0.f};
    axes[1] = glm::vec4{0.f, 0.f, 1.f, 0.f};
    axes[2] = glm::vec4{-1.f, 0.f, 0.f, 0.f};
    return glm::inverse(quakeView) * glm::scale(glm::mat4{1.f}, glm::vec3{unitsPerMetre}) *
        axes * glm::inverse(stageFromEye);
}

// The application-applied transform taking previous application-space surface
// coordinates into the current application space, independently of tracked motion.
[[nodiscard]] inline glm::mat4 applicationDelta(const glm::mat4& previousWorldFromStage,
    const glm::mat4& currentWorldFromStage)
{
    return glm::inverse(currentWorldFromStage) * previousWorldFromStage;
}

// A rigid application-space transform must be shared by both projection views.
// This rejects per-eye deltas produced by non-rigid camera blends (for example,
// interpolating eye positions with a full yaw while interpolating eye angles
// with a fractional yaw). Translation is in metres in the stage mapping.
[[nodiscard]] inline bool sameApplicationDelta(const glm::mat4& left, const glm::mat4& right,
    float translationToleranceMetres = 0.001f, float rotationToleranceRadians = 0.00174532925f)
{
    const glm::vec3 leftPosition{left[3]}, rightPosition{right[3]};
    if(!std::isfinite(leftPosition.x) || !std::isfinite(leftPosition.y) || !std::isfinite(leftPosition.z) ||
        !std::isfinite(rightPosition.x) || !std::isfinite(rightPosition.y) || !std::isfinite(rightPosition.z) ||
        glm::length(leftPosition - rightPosition) > translationToleranceMetres)
    {
        return false;
    }
    const glm::quat leftRotation = glm::normalize(glm::quat_cast(glm::mat3{left}));
    const glm::quat rightRotation = glm::normalize(glm::quat_cast(glm::mat3{right}));
    const float dot = std::abs(glm::dot(leftRotation, rightRotation)); // q and -q encode the same rotation
    return std::isfinite(dot) && dot >= std::cos(rotationToleranceRadians * 0.5f);
}

[[nodiscard]] inline bool continuousHistory(double producedTime, double submittedTime,
    double currentTime, bool sameWorld)
{
    return sameWorld && producedTime >= 0.0 && std::isfinite(producedTime) &&
        std::isfinite(submittedTime) && std::isfinite(currentTime) &&
        std::abs(producedTime - submittedTime) < 1e-6 && currentTime > producedTime &&
        currentTime - producedTime < 0.1;
}

// The existing VR_OverrideProjection arithmetic, with Quake view axes x forward,
// y left, z up. Keep clip w and all unrelated entries from GL_FrustumMatrix.
inline void overrideEyeProjection(float matrix[16], float l, float r, float u, float d,
    float n, float f, bool reversedZeroToOne)
{
    matrix[1 * 4 + 0] = -2.f / (r - l);
    matrix[0 * 4 + 0] = -(r + l) / (r - l);
    matrix[2 * 4 + 1] = 2.f / (u - d);
    matrix[0 * 4 + 1] = -(u + d) / (u - d);
    if(reversedZeroToOne)
    {
        matrix[0 * 4 + 2] = -n / (f - n);
        matrix[3 * 4 + 2] = f * n / (f - n);
    }
    else
    {
        matrix[0 * 4 + 2] = (f + n) / (f - n);
        matrix[3 * 4 + 2] = -2.f * f * n / (f - n);
    }
}

[[nodiscard]] inline bool finite(const glm::vec3& v)
{
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

[[nodiscard]] inline bool finite(const glm::vec4& v)
{
    return finite(glm::vec3{v}) && std::isfinite(v.w);
}

struct StaticReprojection
{
    glm::vec3 world{0.f};
    glm::vec2 previousUv{0.f}; // OpenGL image UV; caller must audit runtime image orientation.
    glm::vec3 ndcDelta{0.f};   // current minus previous, with window-depth Z in 0..1.
};

// Opaque finite geometry only: clear-depth sky, hidden-area pixels, dynamic
// surfaces and postprocess distortion need separate classification/handling.
// A rejected sample must not be submitted as a fabricated zero vector.
[[nodiscard]] inline bool reprojectStatic(const glm::mat4& inverseCurrentViewProjection,
    const glm::mat4& previousViewProjection, const glm::vec2& currentUv, float depth,
    bool reversedZeroToOne, StaticReprojection& out)
{
    out = {};
    if(!std::isfinite(currentUv.x) || !std::isfinite(currentUv.y) || !std::isfinite(depth) ||
        currentUv.x < 0.f || currentUv.x > 1.f || currentUv.y < 0.f || currentUv.y > 1.f ||
        depth < 0.f || depth > 1.f || depth == (reversedZeroToOne ? 0.f : 1.f))
    {
        return false;
    }
    const glm::vec3 currentNdc{currentUv * 2.f - 1.f, depth};
    const float clipZ = reversedZeroToOne ? depth : depth * 2.f - 1.f;
    const glm::vec4 world = inverseCurrentViewProjection * glm::vec4{currentNdc.x, currentNdc.y, clipZ, 1.f};
    if(!finite(world) || std::abs(world.w) < 1e-8f)
    {
        return false;
    }
    const glm::vec3 position = glm::vec3{world} / world.w;
    const glm::vec4 previousClip = previousViewProjection * glm::vec4{position, 1.f};
    if(!finite(position) || !finite(previousClip) || previousClip.w <= 1e-8f)
    {
        return false;
    }
    glm::vec3 previousNdc = glm::vec3{previousClip} / previousClip.w;
    if(!reversedZeroToOne)
    {
        previousNdc.z = previousNdc.z * 0.5f + 0.5f;
    }
    if(!finite(previousNdc))
    {
        return false;
    }
    out.world = position;
    out.previousUv = glm::vec2{previousNdc} * 0.5f + 0.5f;
    out.ndcDelta = currentNdc - previousNdc;
    return finite(out.ndcDelta);
}

// EXT's scale/offset convention for a hypothetical linear UNORM buffer. This
// tests encoding math only; it does not establish a suitable runtime GL format.
// Overflow fails explicitly instead of clipping motion to an incorrect value.
[[nodiscard]] inline bool encodeUnormMotion(const glm::vec3& delta, const glm::vec3& scale,
    const glm::vec3& offset, glm::vec3& encoded)
{
    encoded = glm::vec3{0.f};
    if(!finite(delta) || !finite(scale) || !finite(offset) ||
        scale.x == 0.f || scale.y == 0.f || scale.z == 0.f)
    {
        return false;
    }
    const glm::vec3 value = (delta - offset) / scale;
    if(!finite(value) || value.x < 0.f || value.x > 1.f || value.y < 0.f || value.y > 1.f ||
        value.z < 0.f || value.z > 1.f)
    {
        return false;
    }
    encoded = value;
    return true;
}

struct Srgb8MotionEncoding
{
    float zero{0.f}; // decoded value of byte 188; exactly representable by the target format
    float scale{0.f};
    float offset{0.f};
    float positiveRange{0.f};
    float negativeRange{0.f};
};

[[nodiscard]] inline float srgbDecode(float encoded)
{
    return encoded <= 0.04045f ? encoded / 12.92f : std::pow((encoded + 0.055f) / 1.055f, 2.4f);
}

[[nodiscard]] inline float srgbEncode(float linear)
{
    return linear <= 0.0031308f ? linear * 12.92f : 1.055f * std::pow(linear, 1.f / 2.4f) - 0.055f;
}

// EXT's affine decode can represent a signed value in an sRGB8 channel if the
// image writer encodes linear samples to sRGB and the runtime samples them with
// sRGB decode. Anchor zero to byte 188; using 0.5 would decode to byte 188's
// 0.502886... and introduce a nonzero motion bias. The asymmetric endpoint is
// unavoidable because no sRGB8 code point decodes to exactly 0.5.
[[nodiscard]] inline bool srgb8MotionEncoding(float positiveRange, Srgb8MotionEncoding& out)
{
    out = {};
    if(!std::isfinite(positiveRange) || positiveRange <= 0.f) return false;
    constexpr float sampleZero = 0.502886458f; // IEC 61966-2-1 decode(188/255)
    out.zero = sampleZero;
    out.scale = positiveRange / (1.f - sampleZero);
    out.offset = -sampleZero * out.scale;
    out.positiveRange = (1.f - sampleZero) * out.scale;
    out.negativeRange = sampleZero * out.scale;
    return std::isfinite(out.scale) && std::isfinite(out.offset) && out.scale > 0.f;
}

[[nodiscard]] inline bool encodeSrgb8Motion(float motion, const Srgb8MotionEncoding& encoding,
    unsigned char& code)
{
    code = 0;
    if(!std::isfinite(motion) || !std::isfinite(encoding.zero) || !std::isfinite(encoding.scale) ||
        !std::isfinite(encoding.offset) || encoding.scale <= 0.f) return false;
    const float linearSample = (motion - encoding.offset) / encoding.scale;
    if(!std::isfinite(linearSample) || linearSample < 0.f || linearSample > 1.f) return false;
    const float encoded = srgbEncode(linearSample);
    if(!std::isfinite(encoded) || encoded < 0.f || encoded > 1.f) return false;
    code = static_cast<unsigned char>(std::lround(encoded * 255.f));
    return true;
}

[[nodiscard]] inline float decodeSrgb8Motion(unsigned char code, const Srgb8MotionEncoding& encoding)
{
    return srgbDecode(static_cast<float>(code) / 255.f) * encoding.scale + encoding.offset;
}

} // namespace qvr::synthesis
