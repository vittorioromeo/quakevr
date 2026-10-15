// CPU-only fixtures for projection, application-space mapping, static-depth
// reprojection and motion encoding. No engine, GL, OpenXR, or game startup.
#include "vr_synthesis_math.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/packing.hpp>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <limits>

namespace
{
void require(bool ok, const char* message)
{
    if(!ok)
    {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}

bool close(float a, float b, float tolerance = 2e-5f)
{
    return std::abs(a - b) <= tolerance;
}

void closeVector(const glm::vec3& a, const glm::vec3& b, const char* message, float tolerance = 2e-5f)
{
    require(close(a.x, b.x, tolerance) && close(a.y, b.y, tolerance) && close(a.z, b.z, tolerance), message);
}

glm::mat4 projection(bool reversed, float l = -1.f, float r = 1.f, float u = 1.f, float d = -1.f)
{
    float matrix[16]{};
    matrix[3] = 1.f; // GL_FrustumMatrix: clip w from Quake forward x.
    qvr::synthesis::overrideEyeProjection(matrix, l, r, u, d, 1.f, 4096.f, reversed);
    return glm::make_mat4(matrix);
}

glm::vec3 project(const glm::mat4& vp, const glm::vec3& point, bool reversed)
{
    const glm::vec4 clip = vp * glm::vec4{point, 1.f};
    require(clip.w > 0.f, "fixture point is in front of eye");
    glm::vec3 ndc = glm::vec3{clip} / clip.w;
    if(!reversed) ndc.z = ndc.z * 0.5f + 0.5f;
    return ndc;
}

qvr::synthesis::StaticReprojection checkReprojection(const glm::mat4& current,
    const glm::mat4& previous, const glm::vec3& world, bool reversed)
{
    const glm::vec3 now = project(current, world, reversed);
    const glm::vec3 before = project(previous, world, reversed);
    qvr::synthesis::StaticReprojection result;
    require(qvr::synthesis::reprojectStatic(glm::inverse(current), previous,
        glm::vec2{now} * 0.5f + 0.5f, now.z, reversed, result), "finite static depth reprojects");
    closeVector(result.world, world, "inverse depth reconstructs original world surface", 2e-4f);
    closeVector(result.ndcDelta, now - before, "reprojection matches independently projected surface");
    require(close(result.previousUv.x, before.x * 0.5f + 0.5f) &&
        close(result.previousUv.y, before.y * 0.5f + 0.5f), "previous UV uses previous eye matrix");
    return result;
}

void projectionEquivalence()
{
    // Keep the pre-refactor arithmetic as a compatibility oracle, then check
    // analytic endpoints separately so this is not the sole correctness test.
    for(bool reversed : {false, true})
        for(int sample = 0; sample < 128; ++sample)
        {
            float expected[16], actual[16];
            for(int i = 0; i < 16; ++i) actual[i] = expected[i] = static_cast<float>(i + 1) / 17.f;
            const float l = -0.5f - sample * 0.003f, r = 0.9f + sample * 0.002f;
            const float u = 1.1f + sample * 0.004f, d = -0.8f - sample * 0.001f;
            const float n = 0.02f + sample * 0.01f, f = 512.f + sample * 32.f;
            expected[4] = -2.f / (r - l);
            expected[0] = -(r + l) / (r - l);
            expected[9] = 2.f / (u - d);
            expected[1] = -(u + d) / (u - d);
            expected[2] = reversed ? -n / (f - n) : (f + n) / (f - n);
            expected[14] = reversed ? f * n / (f - n) : -2.f * f * n / (f - n);
            qvr::synthesis::overrideEyeProjection(actual, l, r, u, d, n, f, reversed);
            require(std::memcmp(actual, expected, sizeof(actual)) == 0,
                "all 16 projection entries byte-equivalent, including untouched entries");
        }
}
} // namespace

int main()
{
    using namespace qvr::synthesis;
    projectionEquivalence();
    Srgb8MotionEncoding srgb{};
    require(srgb8MotionEncoding(0.0625f, srgb), "positive sRGB8 motion range creates an affine encoding");
    require(close(srgb.offset + srgb.zero * srgb.scale, 0.f, 2e-6f), "sRGB byte 188 maps exactly to zero motion");
    require(close(srgb.positiveRange, 0.0625f, 2e-6f), "positive endpoint reaches configured motion range");
    require(srgb.negativeRange > srgb.positiveRange && srgb.negativeRange < srgb.positiveRange * 1.02f,
        "zero-anchored sRGB code uses a small asymmetric negative headroom");
    for(const float motion : {-0.06f, -0.02f, -0.001f, 0.f, 0.001f, 0.02f, 0.06f})
    {
        unsigned char code = 0;
        require(encodeSrgb8Motion(motion, srgb, code), "in-range signed motion is encodable");
        const float decoded = decodeSrgb8Motion(code, srgb);
        require(std::abs(decoded - motion) < 0.0012f, "sRGB8 round-trip error stays within the measured NDC budget");
    }
    unsigned char rejected = 0;
    require(!encodeSrgb8Motion(0.07f, srgb, rejected), "out-of-range motion is rejected instead of clipped");
    require(!srgb8MotionEncoding(0.f, srgb), "zero motion range is rejected");
    require(continuousHistory(10.0, 10.0, 10.014, true), "continuous successfully submitted display interval");
    require(!continuousHistory(10.0, 10.014, 10.028, true), "empty/held/unproduced intervening frame invalidates history");
    require(!continuousHistory(10.0, 10.0, 10.014, false), "map generation invalidates history");
    require(!continuousHistory(10.0, 10.0, 10.2, true), "long gap invalidates history");
    require(!continuousHistory(10.0, 10.0, 10.0, true), "repeated display time invalidates history");
    glm::mat4 basis{1.f};
    basis[0] = {0.f, -1.f, 0.f, 0.f};
    basis[1] = {0.f, 0.f, 1.f, 0.f};
    basis[2] = {-1.f, 0.f, 0.f, 0.f};
    const float units = 1.f / 0.0381f;
    const glm::mat4 mapping = glm::scale(glm::mat4{1.f}, glm::vec3{units}) * basis;
    const glm::mat4 tracking = glm::translate(glm::mat4{1.f}, {0.2f, 0.3f, -0.1f}) *
        glm::rotate(glm::mat4{1.f}, 0.2f, {0.f, 1.f, 0.f});
    const glm::mat4 actualCamera = mapping * tracking * glm::inverse(mapping);
    const glm::mat4 reconstructed = worldFromStage(glm::inverse(actualCamera), tracking, units);
    for(int column = 0; column < 4; ++column)
        closeVector(glm::vec3{reconstructed[column]}, glm::vec3{mapping[column]}, "tracked motion cancels from application mapping", 1e-4f);
    const glm::mat4 moved = glm::translate(glm::mat4{1.f}, {units, 0.f, 0.f}) * mapping;
    const glm::mat4 appDelta = applicationDelta(mapping, moved);
    closeVector(glm::vec3{appDelta[3]}, {0.f, 0.f, 1.f}, "application forward movement gives metric current-space surface transform");
    const glm::vec4 world{52.f, 3.f, 4.f, 1.f};
    closeVector(glm::vec3{appDelta * glm::inverse(mapping) * world}, glm::vec3{glm::inverse(moved) * world},
        "application delta reexpresses previous-space surface in current space");
    const glm::mat4 previousHead = glm::translate(glm::mat4{1.f}, {0.15f, -0.2f, 0.1f}) *
        glm::rotate(glm::mat4{1.f}, -0.12f, {0.f, 0.f, 1.f});
    const glm::mat4 currentHead = glm::translate(glm::mat4{1.f}, {-0.1f, 0.25f, -0.05f}) *
        glm::rotate(glm::mat4{1.f}, 0.31f, {0.f, 1.f, 0.f});
    glm::mat4 eyeDeltas[2];
    for(int eye = 0; eye < 2; ++eye)
    {
        const float eyeOffset = eye == 0 ? -0.032f : 0.032f;
        const glm::mat4 eyeOffsetTransform = glm::translate(glm::mat4{1.f}, {eyeOffset, 0.f, 0.f});
        const glm::mat4 previousTrackedEye = previousHead * eyeOffsetTransform;
        const glm::mat4 currentTrackedEye = currentHead * eyeOffsetTransform;
        const glm::mat4 previousCamera = mapping * previousTrackedEye * glm::inverse(mapping);
        const glm::mat4 currentCamera = moved * currentTrackedEye * glm::inverse(mapping);
        const glm::mat4 previousEyeMapping = worldFromStage(glm::inverse(previousCamera), previousTrackedEye, units);
        const glm::mat4 currentEyeMapping = worldFromStage(glm::inverse(currentCamera), currentTrackedEye, units);
        eyeDeltas[eye] = applicationDelta(previousEyeMapping, currentEyeMapping);
        closeVector(glm::vec3{eyeDeltas[eye][3]}, glm::vec3{appDelta[3]},
            "app-space delta is metric and consistent for each eye despite head movement");
    }
    closeVector(glm::vec3{eyeDeltas[0][0]}, glm::vec3{eyeDeltas[1][0]},
        "both eyes submit the same application-space delta rotation");
    closeVector(glm::vec3{eyeDeltas[0][1]}, glm::vec3{eyeDeltas[1][1]},
        "both eyes submit the same application-space delta rotation");
    closeVector(glm::vec3{eyeDeltas[0][2]}, glm::vec3{eyeDeltas[1][2]},
        "both eyes submit the same application-space delta rotation");
    require(sameApplicationDelta(eyeDeltas[0], eyeDeltas[1]),
        "tracked stereo eyes produce a shared rigid application-space delta");
    const glm::mat4 nonRigidLeft = glm::translate(glm::mat4{1.f}, {-0.00935f, 0.f, 0.f}) *
        glm::rotate(glm::mat4{1.f}, glm::radians(45.f), {0.f, 0.f, 1.f});
    const glm::mat4 nonRigidRight = glm::translate(glm::mat4{1.f}, {0.00935f, 0.f, 0.f}) *
        glm::rotate(glm::mat4{1.f}, glm::radians(45.f), {0.f, 0.f, 1.f});
    require(!sameApplicationDelta(nonRigidLeft, nonRigidRight),
        "different eye mappings from a fractional-yaw camera blend are rejected");
    glm::mat4 oppositeQuaternion = eyeDeltas[0];
    const glm::quat q = glm::normalize(glm::quat_cast(glm::mat3{oppositeQuaternion}));
    oppositeQuaternion = glm::mat4_cast(-q);
    oppositeQuaternion[3] = eyeDeltas[0][3];
    require(sameApplicationDelta(eyeDeltas[0], oppositeQuaternion),
        "quaternion sign ambiguity does not reject an equivalent rotation");
    for(bool reversed : {false, true})
    {
        const glm::mat4 p = projection(reversed);
        require(close(project(p, {1.f, 0.f, 0.f}, reversed).z, reversed ? 1.f : 0.f), "near depth endpoint");
        require(close(project(p, {4096.f, 0.f, 0.f}, reversed).z, reversed ? 0.f : 1.f), "far depth endpoint");
        const glm::mat4 asymmetric = projection(reversed, -0.8f, 1.2f, 1.1f, -0.9f);
        require(close(project(asymmetric, {8.f, -9.6f, 0.f}, reversed).x, 1.f), "asymmetric right tangent");
        require(close(project(asymmetric, {8.f, 6.4f, 0.f}, reversed).x, -1.f), "asymmetric left tangent");
        require(close(project(asymmetric, {8.f, 0.f, 8.8f}, reversed).y, 1.f), "asymmetric upper tangent");
        require(close(project(asymmetric, {8.f, 0.f, -7.2f}, reversed).y, -1.f), "asymmetric lower tangent");
        closeVector(checkReprojection(p, p, {16.f, 0.f, 0.f}, reversed).ndcDelta,
            {0.f, 0.f, 0.f}, "stationary surface gives zero motion");
        const glm::mat4 current = p * glm::translate(glm::mat4{1.f}, {0.f, -2.f, -1.f});
        const auto translated = checkReprojection(current, p, {16.f, 0.f, 0.f}, reversed);
        closeVector(translated.ndcDelta, {0.125f, -0.0625f, 0.f}, "Quake translation and current-minus-previous sign");
        require(close(translated.previousUv.x, 0.5f) && close(translated.previousUv.y, 0.5f), "reprojection fetch returns prior pixel");
        const auto forward = checkReprojection(p * glm::translate(glm::mat4{1.f}, {-2.f, 0.f, 0.f}),
            p, {16.f, 0.f, 0.f}, reversed);
        require(reversed ? forward.ndcDelta.z > 0.f : forward.ndcDelta.z < 0.f, "depth motion follows Z convention");
        checkReprojection(p * glm::rotate(glm::mat4{1.f}, glm::radians(-45.f), {0.f, 0.f, 1.f}),
            p, {16.f, 16.f, 0.f}, reversed);

        glm::mat4 eyes[2];
        for(int eye = 0; eye < 2; ++eye)
        {
            const float y = eye == 0 ? 0.8f : -0.8f;
            eyes[eye] = projection(reversed, eye == 0 ? -1.15f : -0.85f,
                eye == 0 ? 0.85f : 1.15f, 1.05f, -0.95f) *
                glm::translate(glm::mat4{1.f}, {0.f, -y, 0.f});
            checkReprojection(eyes[eye] * glm::translate(glm::mat4{1.f}, {0.f, -2.f, -1.f}),
                eyes[eye], {20.f, -3.f, 4.f}, reversed);
        }
        const auto wrongEye = checkReprojection(eyes[1], eyes[0], {20.f, -3.f, 4.f}, reversed);
        require(std::abs(wrongEye.ndcDelta.x) > 0.1f, "fixture detects cross-eye history contamination");

        StaticReprojection invalid;
        require(!reprojectStatic(glm::inverse(p), p, {0.5f, 0.5f}, reversed ? 0.f : 1.f, reversed, invalid), "clear sky is not a finite surface");
        require(!reprojectStatic(glm::inverse(p), p, {0.5f, 0.5f}, -0.1f, reversed, invalid), "reject invalid depth");
        require(!reprojectStatic(glm::mat4{0.f}, p, {0.5f, 0.5f}, 0.5f, reversed, invalid), "reject invalid inverse matrix");
        require(!reprojectStatic(glm::inverse(p), p * glm::translate(glm::mat4{1.f}, {-32.f, 0.f, 0.f}),
            {0.5f, 0.5f}, project(p, {16.f, 0.f, 0.f}, reversed).z, reversed, invalid), "reject behind-previous-eye projection");
    }
    glm::vec3 encoded;
    const glm::vec3 delta{0.125f, -0.5f, 1.25f}, scale{4.f}, offset{-2.f};
    require(encodeUnormMotion(delta, scale, offset, encoded), "signed motion encodes with scale and offset");
    closeVector(encoded * scale + offset, delta, "EXT decoding recovers signed motion");
    require(!encodeUnormMotion({3.f, 0.f, 0.f}, scale, offset, encoded), "overflow must not silently clip");
    require(!encodeUnormMotion(delta, {0.f, 4.f, 4.f}, offset, encoded), "reject zero encoding scale");
    require(!encodeUnormMotion({std::numeric_limits<float>::quiet_NaN(), 0.f, 0.f}, scale, offset, encoded), "reject nonfinite motion");
    const glm::vec2 signedMotion{-3.25f, 0.125f};
    const glm::vec2 half = glm::unpackHalf2x16(glm::packHalf2x16(signedMotion));
    require(half == signedMotion, "signed half-float preserves negative and beyond-NDC-range motion");
    std::puts("PASS: projection bytes, both depth conventions, asymmetric stereo, static reprojection, motion sign/encoding, application mapping, submitted history and invalid inputs");
}
