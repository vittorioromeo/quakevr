#pragma once

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
