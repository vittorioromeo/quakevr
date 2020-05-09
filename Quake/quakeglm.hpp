#pragma once

#ifdef __GNUC__
#pragma GCC diagnostic push

#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wmacro-redefined"
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#define GLM_FORCE_INLINE
#define GLM_CONFIG_SIMD GLM_ENABLE
#define GLM_CONFIG_SWIZZLE GLM_SWIZZLE_OPERATOR
#define GLM_CONFIG_ALIGNED_GENTYPES GLM_ENABLE
#define GLM_CONFIG_ANONYMOUS_STRUCT GLM_ENABLE
#define GLM_FORCE_SWIZZLE

#include <GL/glew.h>

#include <SDL2/SDL_opengl.h>

#include <glm.hpp>
#include <gtc/type_ptr.hpp>
#include <gtc/quaternion.hpp>
#include <gtx/quaternion.hpp>
#include <gtx/euler_angles.hpp>
#include <gtx/rotate_vector.hpp>
#include <gtx/io.hpp>
#include <gtx/vec_swizzle.hpp>

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

extern template struct glm::vec<4, signed char, glm::packed_highp>;
extern template struct glm::vec<4, unsigned char, glm::packed_highp>;
extern template struct glm::vec<4, float, glm::packed_highp>;
extern template struct glm::vec<3, float, glm::packed_highp>;
extern template struct glm::vec<2, float, glm::packed_highp>;

#if 0

using qfloat = double;
using qvec2 = glm::dvec2;
using qvec3 = glm::dvec3;
using qmat3 = glm::dmat3;
using qquat = glm::dquat;

[[nodiscard]] inline const GLfloat* toGlVec(const qvec3& v) noexcept
{
    // TODO VR: (P2) UB? stack ptr
    glm::vec3 fv(v);
    return glm::value_ptr(fv);
}

[[nodiscard]] inline const GLfloat* toGlMat(const qquat& q) noexcept
{
    // TODO VR: (P2) UB? stack ptr
    glm::fquat fq(q);
    return &glm::mat4_cast(fq)[0][0];
}

#else

using qfloat = float;
using qvec2 = glm::vec2;
using qvec3 = glm::vec3;
using qvec4 = glm::vec4;
using qmat3 = glm::mat3;
using qquat = glm::quat;

[[nodiscard]] inline auto toGlMat(const qquat& q) noexcept
{
    return glm::mat4_cast(q);
}

#endif

using qfvec3 = glm::vec3;

[[nodiscard]] inline GLfloat* toGlVec(qfvec3& v) noexcept
{
    return glm::value_ptr(v);
}

[[nodiscard]] inline const GLfloat* toGlVec(const qfvec3& v) noexcept
{
    return glm::value_ptr(v);
}

[[nodiscard]] constexpr inline qfloat operator"" _qf(long double x) noexcept
{
    return qfloat(x);
}

// TODO VR: (P2) move to their own header

#if !defined(QUAKE_FORCEINLINE)
#if defined(__GNUC__) || defined(__clang__)
#define QUAKE_FORCEINLINE [[gnu::always_inline]] inline
#elif defined(_MSC_VER)
#define QUAKE_FORCEINLINE __forceinline
#else
#define QUAKE_FORCEINLINE inline
#endif
#endif

#if !defined(QUAKE_PURE)
#if defined(__GNUC__) || defined(__clang__)
#define QUAKE_PUREFN [[gnu::pure]]
#else
#define QUAKE_PUREFN
#endif
#endif

#if !defined(QUAKE_CONSTFN)
#if defined(__GNUC__) || defined(__clang__)
#define QUAKE_CONSTFN [[gnu::const]]
#else
#define QUAKE_CONSTFN
#endif
#endif

#if !defined(QUAKE_FORCEINLINE_PUREFN)
#define QUAKE_FORCEINLINE_PUREFN QUAKE_PUREFN QUAKE_FORCEINLINE
#endif

#if !defined(QUAKE_FORCEINLINE_CONSTFN)
#define QUAKE_FORCEINLINE_CONSTFN QUAKE_CONSTFN QUAKE_FORCEINLINE
#endif
