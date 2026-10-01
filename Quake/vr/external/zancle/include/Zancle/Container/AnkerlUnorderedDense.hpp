// clang-format off

// Small fork of ankerl::unordered_dense to use Zancle's base library.
// Removes allocator support.

///////////////////////// ankerl::unordered_dense::{map, set} /////////////////////////

// A fast & densely stored hashmap and hashset based on robin-hood backward shift deletion.
// Version 4.8.1
// https://github.com/martinus/unordered_dense
//
// Licensed under the MIT License <http://opensource.org/licenses/MIT>.
// SPDX-License-Identifier: MIT
// Copyright (c) 2022-2024 Martin Leitner-Ankerl <martin.ankerl@gmail.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

// NOLINTBEGIN(readability-identifier-naming)

#pragma once
#pragma GCC system_header


#if defined(__GNUC__)
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#    define ANKERL_UNORDERED_DENSE_PACK(decl) decl __attribute__((__packed__))
#elif defined(_MSC_VER)
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#    define ANKERL_UNORDERED_DENSE_PACK(decl) __pragma(pack(push, 1)) decl __pragma(pack(pop))
#endif

#include "Zancle/Base/Abort.hpp"
#include "Zancle/Base/Memcpy.hpp"
#include "Zancle/Base/Memset.hpp"
#include "Zancle/Base/Exchange.hpp"
#include "Zancle/Base/FwdStdHash.hpp"
#include "Zancle/Base/InitializerList.hpp"
#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Base/Macros.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Vocabulary/Optional.hpp"
#include "Zancle/Base/PlacementNew.hpp"
#include "Zancle/Base/PtrDiffT.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Base/Swap.hpp"
#include "Zancle/Trait/Conditional.hpp"
#include "Zancle/Trait/DeclVal.hpp"
#include "Zancle/Trait/EnableTrivialRelocation.hpp"
#include "Zancle/Trait/IsConstructible.hpp"
#include "Zancle/Trait/IsConvertible.hpp"
#include "Zancle/Trait/IsEnum.hpp"
#include "Zancle/Trait/IsNothrowMoveAssignable.hpp"
#include "Zancle/Trait/IsNothrowSwappable.hpp"
#include "Zancle/Trait/IsSame.hpp"
#include "Zancle/Trait/IsTriviallyCopyable.hpp"
#include "Zancle/Trait/IsTriviallyDestructible.hpp"
#include "Zancle/Trait/IsTriviallyRelocatable.hpp"
#include "Zancle/Trait/IsVoid.hpp"
#include "Zancle/Trait/RemoveCVRef.hpp"
#include "Zancle/Trait/UnderlyingType.hpp"
#include "Zancle/Base/UIntPtrT.hpp"
#include "Zancle/Container/Vector.hpp"

#    if defined(_MSC_VER) && defined(_M_X64) && !defined(__SIZEOF_INT128__)
// Declared directly instead of including the heavy `<intrin.h>`.
extern "C" unsigned __int64 _umul128(unsigned __int64, unsigned __int64, unsigned __int64*);
#        pragma intrinsic(_umul128)
#    endif

namespace ankerl::unordered_dense::inline v4_8_1 {

namespace detail {

    struct piecewise_fn { };

    template <typename A, typename B>
    struct pair {
        ZA_ENABLE_TRIVIAL_RELOCATION_IF(za::isTriviallyRelocatable<A> && za::isTriviallyRelocatable<B>);

        A first;
        B second;

        pair() = default;

        template <typename U1, typename U2>
            requires(!za::isSame<za::RemoveCVRefIndirect<U1>, piecewise_fn>)
        pair(U1&& a, U2&& b)
            : first(ZA_FORWARD(a))
            , second(ZA_FORWARD(b))
        { }

        // Direct-initializes both members (like `std::pair`'s piecewise constructor): no C-style casts
        // for single arguments, and `second` is value-initialized when `args` is empty.
        template <typename K, typename... Args>
        pair(piecewise_fn, K&& k, Args&&... args)
            : first(ZA_FORWARD(k))
            , second(ZA_FORWARD(args)...)
        { }
    };

    template <typename T>
    using vector = za::Vector<T>;

template <typename Iter>
[[gnu::always_inline]] constexpr za::PtrDiffT my_distance(Iter first, Iter last)
{
    return static_cast<za::PtrDiffT>(last - first);
}

// Only used by the (currently disabled) `segmented_vector` below.
#if 0
template <typename T>
class my_allocator {
public:
    using value_type = T;
    using pointer = T*;
    using const_pointer = const T*;
    using reference = T&;
    using const_reference = const T&;
    using size_type = za::SizeT;
    using difference_type = za::PtrDiffT;

    template <typename U>
    struct rebind {
        using other = my_allocator<U>;
    };

    constexpr my_allocator() noexcept = default;

    template <typename U>
    constexpr my_allocator(const my_allocator<U>& /*other*/) noexcept {}

    ~my_allocator() = default;

    [[nodiscard]] pointer allocate(size_type n) {
        if (n == 0) {
            return nullptr; // Standard behavior for n=0 often implementation-defined, nullptr is safe.
        }

        if (n > max_size()) {
            za::abort();
        }

        void* raw_mem = ::operator new(n * sizeof(T));
        return static_cast<pointer>(raw_mem);
    }

    void deallocate(pointer p, size_type /*n*/) noexcept {
        ::operator delete(p);
    }

    template <typename U, typename... Args>
    void construct(U* p, Args&&... args) {
        ZA_PLACEMENT_NEW(static_cast<void*>(p)) U(ZA_FORWARD(args)...);
    }

    template <typename U>
    void destroy(U* p) noexcept {
        p->~U();
    }

    [[nodiscard]] constexpr size_type max_size() const noexcept {
        return static_cast<size_type>(-1) / sizeof(T);
    }

    pointer address(reference x) const noexcept {
        return &x;
    }

    const_pointer address(const_reference x) const noexcept {
        return &x;
    }

    template <typename U>
    constexpr bool operator==(const my_allocator<U>& /*other*/) const noexcept {
        return true;
    }
};

template <>
class my_allocator<void> {
public:
    using value_type = void;
    using pointer = void*;
    using const_pointer = const void*;
    using size_type = za::SizeT;
    using difference_type = za::PtrDiffT;

    template <typename U>
    struct rebind {
        using other = my_allocator<U>;
    };

    template <typename U>
    constexpr my_allocator(const my_allocator<U>& /*other*/) noexcept {}
    constexpr my_allocator() noexcept = default;


    template <typename U>
    constexpr bool operator==(const my_allocator<U>& /*other*/) const noexcept {
        return true; // All stateless allocators are equal
    }
};
#endif


template<typename T>
struct EqualTo
{
  [[nodiscard, gnu::always_inline]] constexpr bool operator()(const T& x, const T& y) const { return x == y; }
};

} // namespace detail

// hash /////////////////////////////////////////////////////////////////////

// This is a stripped-down implementation of wyhash: https://github.com/wangyi-fudan/wyhash
// No big-endian support (because different values on different machines don't matter),
// hardcodes seed and the secret, reformats the code, and clang-tidy fixes.
namespace detail::wyhash {

[[gnu::always_inline]] inline void mum(za::U64* a, za::U64* b) {
#    if defined(__SIZEOF_INT128__)
    __uint128_t r = *a;
    r *= *b;
    *a = static_cast<za::U64>(r);
    *b = static_cast<za::U64>(r >> 64U);
#    elif defined(_MSC_VER) && defined(_M_X64)
    *a = _umul128(*a, *b, b);
#    else
    za::U64 ha = *a >> 32U;
    za::U64 hb = *b >> 32U;
    za::U64 la = static_cast<za::U32>(*a);
    za::U64 lb = static_cast<za::U32>(*b);
    za::U64 hi{};
    za::U64 lo{};
    za::U64 rh = ha * hb;
    za::U64 rm0 = ha * lb;
    za::U64 rm1 = hb * la;
    za::U64 rl = la * lb;
    za::U64 t = rl + (rm0 << 32U);
    auto c = static_cast<za::U64>(t < rl);
    lo = t + (rm1 << 32U);
    c += static_cast<za::U64>(lo < t);
    hi = rh + (rm0 >> 32U) + (rm1 >> 32U) + c;
    *a = lo;
    *b = hi;
#    endif
}

// multiply and xor mix function, aka MUM
[[nodiscard,gnu::always_inline]] inline auto mix(za::U64 a, za::U64 b) -> za::U64 {
    mum(&a, &b);
    return a ^ b;
}

// read functions. WARNING: we don't care about endianness, so results are different on big endian!
[[nodiscard,gnu::always_inline]] inline auto r8(const za::U8* p) -> za::U64 {
    za::U64 v;  // NOLINT(cppcoreguidelines-init-variables)
    ZA_MEMCPY(&v, p, 8U);
    return v;
}

[[nodiscard,gnu::always_inline]] inline auto r4(const za::U8* p) -> za::U64 {
    za::U32 v; // NOLINT(cppcoreguidelines-init-variables)
    ZA_MEMCPY(&v, p, 4);
    return v;
}

// reads 1, 2, or 3 bytes
[[nodiscard,gnu::always_inline]] inline auto r3(const za::U8* p, za::SizeT k) -> za::U64 {
    return (static_cast<za::U64>(p[0]) << 16U) | (static_cast<za::U64>(p[k >> 1U]) << 8U) | p[k - 1];
}

[[maybe_unused]] [[nodiscard]] inline auto hash(void const* key, za::SizeT len) -> za::U64 {
    static constexpr za::U64 secret[4] = {0xa0761d6478bd642f,
                                              0xe7037ed1a0b428db,
                                              0x8ebc6af09c88c6e3,
                                              0x589965cc75374cc3};

    auto const* p = static_cast<za::U8 const*>(key);
    za::U64 seed = secret[0];
    za::U64 a{};
    za::U64 b{};
    if (len <= 16) [[likely]] {
        if (len >= 4) [[likely]] {
            a = (r4(p) << 32U) | r4(p + ((len >> 3U) << 2U));
            b = (r4(p + len - 4) << 32U) | r4(p + len - 4 - ((len >> 3U) << 2U));
        } else if (len > 0) [[likely]] {
            a = r3(p, len);
            b = 0;
        } else {
            a = 0;
            b = 0;
        }
    } else {
        za::SizeT i = len;
        if (i > 48) [[unlikely]] {
            za::U64 see1 = seed;
            za::U64 see2 = seed;
            do {
                seed = mix(r8(p) ^ secret[1], r8(p + 8) ^ seed);
                see1 = mix(r8(p + 16) ^ secret[2], r8(p + 24) ^ see1);
                see2 = mix(r8(p + 32) ^ secret[3], r8(p + 40) ^ see2);
                p += 48;
                i -= 48;
            } while (i > 48);
            seed ^= see1 ^ see2;
        }
        while (i > 16) {
            seed = mix(r8(p) ^ secret[1], r8(p + 8) ^ seed);
            i -= 16;
            p += 16;
        }
        a = r8(p + i - 16);
        b = r8(p + i - 8);
    }

    return mix(secret[1] ^ len, mix(a ^ secret[1], b ^ seed));
}

[[nodiscard, gnu::always_inline]] inline auto hash(za::U64 x) -> za::U64 {
    return detail::wyhash::mix(x, za::U64(0x9E3779B97F4A7C15));
}

} // namespace detail::wyhash

template <typename T, typename Enable = void>
struct hash {
    auto operator()(T const& obj) const noexcept(noexcept(za::declVal<std::hash<T>>().operator()(za::declVal<T const&>())))
        -> za::U64 {
        return std::hash<T>{}(obj);
    }
};

template <typename T>
    requires requires { typename std::hash<T>::is_avalanching; }
struct hash<T> {
    using is_avalanching = void;
    auto operator()(T const& obj) const noexcept(noexcept(za::declVal<std::hash<T>>().operator()(za::declVal<T const&>())))
        -> za::U64 {
        return std::hash<T>{}(obj);
    }
};

template <class T>
struct hash<T*> {
    using is_avalanching = void;
    [[nodiscard, gnu::always_inline]] auto operator()(T* ptr) const noexcept -> za::U64 {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        return detail::wyhash::hash(reinterpret_cast<za::UIntPtrT>(ptr));
    }
};

template <typename Enum>
    requires ZA_IS_ENUM(Enum)
struct hash<Enum> {
    using is_avalanching = void;
    [[nodiscard, gnu::always_inline]] auto operator()(Enum e) const noexcept -> za::U64 {
        using underlying = ZA_UNDERLYING_TYPE(Enum);
        return detail::wyhash::hash(static_cast<underlying>(e));
    }
};

namespace detail {

// Contiguous ranges exposing `data()`/`size()` (strings, string views, vectors, ...) whose elements can be
// hashed by their raw bytes: equal elements must have equal object representations (no padding bytes, no
// `+0.0`/`-0.0`, no owning pointers). Holds for all character and integer types.
template <typename T>
concept byte_hashable_range = requires(const T& r) {
    r.data();
    r.size();
} && __has_unique_object_representations(ZA_REMOVE_CVREF(decltype(*za::declVal<const T&>().data())));

} // namespace detail

template <typename StringLike>
    requires detail::byte_hashable_range<StringLike>
struct hash<StringLike> {
    using is_avalanching = void;
    [[nodiscard, gnu::always_inline]] auto operator()(const StringLike& stringLike) const noexcept -> za::U64 {
        return detail::wyhash::hash(stringLike.data(), sizeof(*stringLike.data()) * stringLike.size());
    }
};

// Strong-typedef-like wrappers: any type that exposes `T::UnderlyingType` and a
// `toUnderlying()` accessor returning something convertible to `za::U64` is hashed
// by running the underlying value through wyhash. Tagged avalanching to skip the
// second-stage mix.
template <typename StrongTypedef>
    requires requires (const StrongTypedef& s) {
        typename StrongTypedef::UnderlyingType;
        { static_cast<za::U64>(s.toUnderlying()) };
    }
struct hash<StrongTypedef> {
    using is_avalanching = void;
    [[nodiscard, gnu::always_inline]] auto operator()(const StrongTypedef& value) const noexcept -> za::U64 {
        return detail::wyhash::hash(static_cast<za::U64>(value.toUnderlying()));
    }
};

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#    define ANKERL_UNORDERED_DENSE_HASH_STATICCAST(T)                                          \
        template <>                                                                            \
        struct hash<T> {                                                                       \
            using is_avalanching = void;                                                       \
            [[nodiscard, gnu::always_inline]] auto operator()(T const& obj) const noexcept -> za::U64 { \
                return detail::wyhash::hash(static_cast<za::U64>(obj));                        \
            }                                                                                  \
        }

#    if defined(__GNUC__) && !defined(__clang__)
#        pragma GCC diagnostic push
#        pragma GCC diagnostic ignored "-Wuseless-cast"
#    endif
// see https://en.cppreference.com/w/cpp/utility/hash
ANKERL_UNORDERED_DENSE_HASH_STATICCAST(bool);
ANKERL_UNORDERED_DENSE_HASH_STATICCAST(char);
ANKERL_UNORDERED_DENSE_HASH_STATICCAST(signed char);
ANKERL_UNORDERED_DENSE_HASH_STATICCAST(unsigned char);
ANKERL_UNORDERED_DENSE_HASH_STATICCAST(char8_t);
ANKERL_UNORDERED_DENSE_HASH_STATICCAST(char16_t);
ANKERL_UNORDERED_DENSE_HASH_STATICCAST(char32_t);
ANKERL_UNORDERED_DENSE_HASH_STATICCAST(wchar_t);
ANKERL_UNORDERED_DENSE_HASH_STATICCAST(short);
ANKERL_UNORDERED_DENSE_HASH_STATICCAST(unsigned short);
ANKERL_UNORDERED_DENSE_HASH_STATICCAST(int);
ANKERL_UNORDERED_DENSE_HASH_STATICCAST(unsigned int);
ANKERL_UNORDERED_DENSE_HASH_STATICCAST(long);
ANKERL_UNORDERED_DENSE_HASH_STATICCAST(long long);
ANKERL_UNORDERED_DENSE_HASH_STATICCAST(unsigned long);
ANKERL_UNORDERED_DENSE_HASH_STATICCAST(unsigned long long);

#    if defined(__GNUC__) && !defined(__clang__)
#        pragma GCC diagnostic pop
#    endif

// Floating-point hashers (stdlib-free).
//
// Strategy: bit-cast the value to an unsigned integer through `ZA_MEMCPY`
// (avoids any `std::bit_cast`/`std::hash`/`<cmath>` dependency) and run it
// through wyhash.
//
// Two subtleties:
// - `+0.0` and `-0.0` compare equal but have different bit patterns. We
//   normalize: any zero (positive or negative) maps to the all-zero bit
//   pattern before hashing.
// - NaN: distinct NaN bit patterns hash to distinct values. Since `NaN != NaN`
//   anyway, this never causes equality lookups to misbehave -- NaN keys are
//   simply unreachable in a hash map, matching `std::hash<float>` behavior.
namespace detail::wyhash {

template <typename Float>
[[nodiscard, gnu::always_inline]] inline auto hashFloat(Float v) noexcept -> za::U64 {
    if constexpr (sizeof(Float) == sizeof(za::U32)) {
        // NOLINTNEXTLINE(cppcoreguidelines-init-variables)
        za::U32 bits;
        ZA_MEMCPY(&bits, &v, sizeof(bits));
        // `bits << 1` strips the sign; the result is 0 only for +0.0/-0.0
        // (subnormals/Inf/NaN all have a non-zero exponent or mantissa).
        if ((bits << 1) == 0u)
            bits = 0u;
        return hash(static_cast<za::U64>(bits));
    } else if constexpr (sizeof(Float) == sizeof(za::U64)) {
        // NOLINTNEXTLINE(cppcoreguidelines-init-variables)
        za::U64 bits;
        ZA_MEMCPY(&bits, &v, sizeof(bits));
        if ((bits << 1) == 0u)
            bits = 0u;
        return hash(bits);
    } else {
        // Wider than 64 bits (e.g. x87 80-bit `long double` stored in 12/16
        // bytes, or IEEE-754 binary128). The sign-bit position differs
        // between x87 (bit 79) and IEEE quad (bit 127), so the bit-twiddling
        // above isn't portable here; fall back to the FP compare. This branch
        // is rare in practice -- most code uses `float`/`double`.
        if (v == Float{0})
            return hash(za::U64{0});

        za::U64 buf[2]{};
#    if defined(__LDBL_MANT_DIG__) && __LDBL_MANT_DIG__ == 64
        // x87 80-bit extended precision: only the first 10 bytes hold the value,
        // the rest is padding with unspecified contents that must not be hashed.
        constexpr za::SizeT valueBytes = ZA_IS_SAME(Float, long double) ? 10u : sizeof(Float);
#    else
        constexpr za::SizeT valueBytes = sizeof(Float);
#    endif
        constexpr za::SizeT n = valueBytes < sizeof(buf) ? valueBytes : sizeof(buf);
        ZA_MEMCPY(buf, &v, n);
        return hash(buf[0] ^ buf[1]);
    }
}

} // namespace detail::wyhash

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#    define ANKERL_UNORDERED_DENSE_HASH_FLOAT(T)                                               \
        template <>                                                                            \
        struct hash<T> {                                                                       \
            using is_avalanching = void;                                                       \
            [[nodiscard, gnu::always_inline]] auto operator()(T const& obj) const noexcept -> za::U64 { \
                return detail::wyhash::hashFloat(obj);                                         \
            }                                                                                  \
        }

ANKERL_UNORDERED_DENSE_HASH_FLOAT(float);
ANKERL_UNORDERED_DENSE_HASH_FLOAT(double);
ANKERL_UNORDERED_DENSE_HASH_FLOAT(long double);

// bucket_type //////////////////////////////////////////////////////////

namespace bucket_type {

struct standard {
    static constexpr za::U32 dist_inc = 1U << 8U;             // skip 1 byte fingerprint
    static constexpr za::U32 fingerprint_mask = dist_inc - 1; // mask for 1 byte of fingerprint

    za::U32 m_dist_and_fingerprint; // upper 3 byte: distance to original bucket. lower byte: fingerprint from hash
    za::U32 m_value_idx;            // index into the m_values vector.
};

ANKERL_UNORDERED_DENSE_PACK(struct big {
    static constexpr za::U32 dist_inc = 1U << 8U;             // skip 1 byte fingerprint
    static constexpr za::U32 fingerprint_mask = dist_inc - 1; // mask for 1 byte of fingerprint

    za::U32 m_dist_and_fingerprint; // upper 3 byte: distance to original bucket. lower byte: fingerprint from hash
    za::SizeT m_value_idx;              // index into the m_values vector.
});

} // namespace bucket_type

namespace detail {

struct default_container_t {};

// enable_if helpers

template <typename Mapped>
constexpr bool is_map_v = !za::isVoid<Mapped>;

template <typename Hash, typename KeyEqual>
constexpr bool is_transparent_v = requires {
    typename Hash::is_transparent;
    typename KeyEqual::is_transparent;
};

template <typename From, typename To1, typename To2>
constexpr bool is_neither_convertible_v = !ZA_IS_CONVERTIBLE(From, To1) && !ZA_IS_CONVERTIBLE(From, To2);

template <typename T>
constexpr bool has_reserve = requires(T& t) { t.reserve(za::SizeT{}); };

// base type for map has mapped_type
template <class T>
struct base_table_type_map
{
    using mapped_type = T;
};

// base type for set doesn't have mapped_type
struct base_table_type_set
{
};

} // namespace detail

// `segmented_vector` and the `segmented_map`/`segmented_set` aliases built on it are currently disabled: they are
// unused, do not compile against `za::Vector` (`popBack`/`shrinkToFit` mismatches) and the defaulted move constructor
// leaves the source owning the same blocks. Kept (instead of deleted) for possible future use.
#if 0
// Very much like std::deque, but faster for indexing (in most cases). As of now this doesn't implement the full
// detail::vector API, but merely what's necessary to work as an underlying container for ankerl::unordered_dense::{map,
// set}. It allocates blocks of equal size and puts them into the m_blocks vector. That means it can grow simply by
// adding a new block to the back of m_blocks, and doesn't double its size like an detail::vector. The disadvantage is
// that memory is not linear and thus there is one more indirection necessary for indexing.
template <typename T, za::SizeT MaxSegmentSizeBytes = 4096>
class segmented_vector
{
    template <bool IsConst>
    class iter_t;

public:
    using pointer         = T*;
    using const_pointer   = const T*;
    using difference_type = za::PtrDiffT;
    using value_type      = T;
    using size_type       = za::SizeT;
    using reference       = T&;
    using const_reference = const T&;
    using iterator        = iter_t<false>;
    using const_iterator  = iter_t<true>;

private:
    detail::vector<pointer> m_blocks{};
    za::SizeT         m_size{};

    // Calculates the maximum number for x in  (s << x) <= max_val
    static constexpr auto num_bits_closest(za::SizeT max_val, za::SizeT s) -> za::SizeT
    {
        auto f = za::SizeT{0};
        while (s << (f + 1) <= max_val)
        {
            ++f;
        }
        return f;
    }

    static constexpr auto num_bits              = num_bits_closest(MaxSegmentSizeBytes, sizeof(T));
    static constexpr auto num_elements_in_block = 1U << num_bits;
    static constexpr auto mask                  = num_elements_in_block - 1U;

    /**
     * Iterator class doubles as const_iterator and iterator
     */
    template <bool IsConst>
    class iter_t
    {
        using ptr_t = typename za::Conditional<IsConst, const segmented_vector::const_pointer*, segmented_vector::pointer*>;
        ptr_t           m_data{};
        za::SizeT m_idx{};

        template <bool B>
        friend class iter_t;

    public:
        using difference_type = segmented_vector::difference_type;
        // Use the outer alias rather than `T` directly: VS2022 hits C2061 with
        // the latter inside a nested class template (upstream issue, MSVC bug).
        using value_type = typename segmented_vector::value_type;
        using reference  = typename za::Conditional<IsConst, const value_type&, value_type&>;
        using pointer = typename za::Conditional<IsConst, segmented_vector::const_pointer, segmented_vector::pointer>;

        iter_t() noexcept = default;

        template <bool OtherIsConst>
        // NOLINTNEXTLINE(google-explicit-constructor,hicpp-explicit-conversions)
        constexpr iter_t(const iter_t<OtherIsConst>& other) noexcept
            requires(IsConst && !OtherIsConst)
            : m_data(other.m_data), m_idx(other.m_idx)
        {
        }

        constexpr iter_t(ptr_t data, za::SizeT idx) noexcept : m_data(data), m_idx(idx)
        {
        }

        template <bool OtherIsConst>
        constexpr auto operator=(const iter_t<OtherIsConst>& other) noexcept -> iter_t&
            requires(IsConst && !OtherIsConst)
        {
            m_data = other.m_data;
            m_idx  = other.m_idx;
            return *this;
        }

        // Random-access iterator ops.

        constexpr auto operator++() noexcept -> iter_t&
        {
            ++m_idx;
            return *this;
        }

        constexpr auto operator++(int) noexcept -> iter_t
        {
            iter_t prev(*this);
            this->operator++();
            return prev;
        }

        constexpr auto operator--() noexcept -> iter_t&
        {
            --m_idx;
            return *this;
        }

        constexpr auto operator--(int) noexcept -> iter_t
        {
            iter_t prev(*this);
            this->operator--();
            return prev;
        }

        [[nodiscard]] constexpr auto operator+(difference_type diff) const noexcept -> iter_t
        {
            return {m_data, static_cast<za::SizeT>(static_cast<difference_type>(m_idx) + diff)};
        }

        constexpr auto operator+=(difference_type diff) noexcept -> iter_t&
        {
            m_idx = static_cast<za::SizeT>(static_cast<difference_type>(m_idx) + diff);
            return *this;
        }

        [[nodiscard]] constexpr auto operator-(difference_type diff) const noexcept -> iter_t
        {
            return {m_data, static_cast<za::SizeT>(static_cast<difference_type>(m_idx) - diff)};
        }

        constexpr auto operator-=(difference_type diff) noexcept -> iter_t&
        {
            m_idx = static_cast<za::SizeT>(static_cast<difference_type>(m_idx) - diff);
            return *this;
        }

        template <bool OtherIsConst>
        [[nodiscard]] constexpr auto operator-(const iter_t<OtherIsConst>& other) const noexcept -> difference_type
        {
            return static_cast<difference_type>(m_idx) - static_cast<difference_type>(other.m_idx);
        }

        constexpr auto operator*() const noexcept -> reference
        {
            return m_data[m_idx >> num_bits][m_idx & mask];
        }

        constexpr auto operator->() const noexcept -> pointer
        {
            return &m_data[m_idx >> num_bits][m_idx & mask];
        }

        template <bool O>
        [[nodiscard]] constexpr auto operator==(const iter_t<O>& o) const noexcept -> bool
        {
            return m_idx == o.m_idx;
        }

        template <bool O>
        [[nodiscard]] constexpr auto operator<(const iter_t<O>& o) const noexcept -> bool
        {
            return m_idx < o.m_idx;
        }

        template <bool O>
        [[nodiscard]] constexpr auto operator>(const iter_t<O>& o) const noexcept -> bool
        {
            return o < *this;
        }

        template <bool O>
        [[nodiscard]] constexpr auto operator<=(const iter_t<O>& o) const noexcept -> bool
        {
            return !(o < *this);
        }

        template <bool O>
        [[nodiscard]] constexpr auto operator>=(const iter_t<O>& o) const noexcept -> bool
        {
            return !(*this < o);
        }
    };

    // slow path: need to allocate a new segment every once in a while
    void increase_capacity()
    {
        auto    ba    = detail::my_allocator<T>{};
        pointer block = ba.allocate(num_elements_in_block);
        m_blocks.pushBack(block);
    }

    // Moves everything from other
    void append_everything_from(segmented_vector&& other)
    {
        reserve(size() + other.size());
        for (auto&& o : other)
        {
            emplace_back(ZA_MOVE(o));
        }
    }

    // Copies everything from other
    void append_everything_from(const segmented_vector& other)
    {
        reserve(size() + other.size());
        for (const auto& o : other)
        {
            emplace_back(o);
        }
    }

    void dealloc()
    {
        auto ba = detail::my_allocator<T>{};
        for (auto ptr : m_blocks)
        {
            ba.deallocate(ptr, num_elements_in_block);
        }
    }

    [[nodiscard]] static constexpr auto calc_num_blocks_for_capacity(za::SizeT capacity)
    {
        return (capacity + num_elements_in_block - 1U) / num_elements_in_block;
    }

    void resize_shrink(za::SizeT new_size)
    {
        if constexpr (!ZA_IS_TRIVIALLY_DESTRUCTIBLE(T))
        {
            for (za::SizeT i = new_size; i < m_size; ++i)
            {
                operator[](i).~T();
            }
        }
        m_size = new_size;
    }

public:
    segmented_vector() = default;

    segmented_vector(segmented_vector&& other) noexcept = default;

    segmented_vector(const segmented_vector& other)
    {
        append_everything_from(other);
    }

    auto operator=(const segmented_vector& other) -> segmented_vector&
    {
        if (this == &other)
        {
            return *this;
        }
        clear();
        append_everything_from(other);
        return *this;
    }

    auto operator=(segmented_vector&& other) noexcept -> segmented_vector&
    {
        clear();
        dealloc();

        m_blocks = ZA_MOVE(other.m_blocks);
        m_size   = za::exchange(other.m_size, {});

        return *this;
    }

    ~segmented_vector()
    {
        clear();
        dealloc();
    }

    [[nodiscard]] constexpr auto size() const -> za::SizeT
    {
        return m_size;
    }

    [[nodiscard]] constexpr auto capacity() const -> za::SizeT
    {
        return m_blocks.size() * num_elements_in_block;
    }

    // Indexing is highly performance critical
    [[nodiscard]] constexpr auto operator[](za::SizeT i) const noexcept -> const T&
    {
        return m_blocks[i >> num_bits][i & mask];
    }

    [[nodiscard]] constexpr auto operator[](za::SizeT i) noexcept -> T&
    {
        return m_blocks[i >> num_bits][i & mask];
    }

    [[nodiscard]] constexpr auto begin() -> iterator
    {
        return {m_blocks.data(), 0U};
    }
    [[nodiscard]] constexpr auto begin() const -> const_iterator
    {
        return {m_blocks.data(), 0U};
    }
    [[nodiscard]] constexpr auto cbegin() const -> const_iterator
    {
        return {m_blocks.data(), 0U};
    }

    [[nodiscard]] constexpr auto end() -> iterator
    {
        return {m_blocks.data(), m_size};
    }
    [[nodiscard]] constexpr auto end() const -> const_iterator
    {
        return {m_blocks.data(), m_size};
    }
    [[nodiscard]] constexpr auto cend() const -> const_iterator
    {
        return {m_blocks.data(), m_size};
    }

    [[nodiscard]] constexpr auto back() -> reference
    {
        return operator[](m_size - 1);
    }
    [[nodiscard]] constexpr auto back() const -> const_reference
    {
        return operator[](m_size - 1);
    }

    void pop_back()
    {
        back().~T();
        --m_size;
    }

    [[nodiscard]] auto empty() const
    {
        return 0 == m_size;
    }

    void reserve(za::SizeT new_capacity)
    {
        m_blocks.reserve(calc_num_blocks_for_capacity(new_capacity));
        while (new_capacity > capacity())
        {
            increase_capacity();
        }
    }

    void resize(za::SizeT count)
    {
        if (count < m_size)
        {
            resize_shrink(count);
        }
        else if (count > m_size)
        {
            const za::SizeT newElems = count - m_size;
            reserve(count);
            for (za::SizeT i = 0; i < newElems; ++i)
            {
                emplace_back();
            }
        }
    }

    void resize(za::SizeT count, const value_type& value)
    {
        if (count < m_size)
        {
            resize_shrink(count);
        }
        else if (count > m_size)
        {
            const za::SizeT newElems = count - m_size;
            reserve(count);
            for (za::SizeT i = 0; i < newElems; ++i)
            {
                emplace_back(value);
            }
        }
    }

    template <class... Args>
    auto emplace_back(Args&&... args) -> reference
    {
        if (m_size == capacity())
        {
            increase_capacity();
        }
        auto* ptr = static_cast<void*>(&operator[](m_size));
        auto& ref = *ZA_PLACEMENT_NEW(ptr) T(ZA_FORWARD(args)...);
        ++m_size;
        return ref;
    }

    // Alias to match Base naming used by `detail::vector`.
    template <class... Args>
    auto emplaceBack(Args&&... args) -> reference
    {
        return emplace_back(ZA_FORWARD(args)...);
    }

    void clear()
    {
        if constexpr (!ZA_IS_TRIVIALLY_DESTRUCTIBLE(T))
        {
            for (za::SizeT i = 0, s = size(); i < s; ++i)
            {
                operator[](i).~T();
            }
        }
        m_size = 0;
    }

    void shrink_to_fit()
    {
        auto ba                  = detail::my_allocator<T>{};
        auto num_blocks_required = calc_num_blocks_for_capacity(m_size);
        while (m_blocks.size() > num_blocks_required)
        {
            ba.deallocate(m_blocks.back(), num_elements_in_block);
            m_blocks.popBack();
        }
        m_blocks.shrinkToFit();
    }
};
#endif

namespace detail
{

// This is it, the table. Doubles as map and set, and uses `void` for T when its used as a set.
template <class Key,
          class T, // when void, treat it as a set.
          class Hash,
          class KeyEqual,
          class Bucket,
          class BucketContainer,
          bool IsSegmented>
class table : public za::Conditional<is_map_v<T>, base_table_type_map<T>, base_table_type_set>
{
    using underlying_value_type     = typename za::Conditional<is_map_v<T>, detail::pair<Key, T>, Key>;
#if 0 // segmented containers are currently disabled, see `segmented_vector`
    using underlying_container_type = za::
        Conditional<IsSegmented, segmented_vector<underlying_value_type>, detail::vector<underlying_value_type>>;
#else
    static_assert(!IsSegmented, "segmented containers are currently disabled, see `segmented_vector`");
    using underlying_container_type = detail::vector<underlying_value_type>;
#endif

public:
    using value_container_type = underlying_container_type;

private:
#if 0 // segmented containers are currently disabled, see `segmented_vector`
    using default_bucket_container_type = za::Conditional<IsSegmented, segmented_vector<Bucket>, detail::vector<Bucket>>;
#else
    using default_bucket_container_type = detail::vector<Bucket>;
#endif

    using bucket_container_type = za::Conditional<ZA_IS_SAME(BucketContainer, detail::default_container_t),
                                                        default_bucket_container_type,
                                                        BucketContainer>;

    static constexpr za::U8 initial_shifts          = 64 - 2; // 2^(64-m_shift) number of buckets
    static constexpr float        default_max_load_factor = 0.8F;

public:
    // No self-references: relocatable by `memcpy` whenever all members are.
    ZA_ENABLE_TRIVIAL_RELOCATION_IF(za::isTriviallyRelocatable<value_container_type> &&
                                    za::isTriviallyRelocatable<bucket_container_type> &&
                                    za::isTriviallyRelocatable<Hash> && za::isTriviallyRelocatable<KeyEqual>);

    using key_type        = Key;
    using value_type      = typename value_container_type::value_type;
    using size_type       = typename value_container_type::size_type;
    using difference_type = typename value_container_type::difference_type;
    using hasher          = Hash;
    using key_equal       = KeyEqual;
    using reference       = typename value_container_type::reference;
    using const_reference = typename value_container_type::const_reference;
    using pointer         = typename value_container_type::pointer;
    using const_pointer   = typename value_container_type::const_pointer;
    using const_iterator  = typename value_container_type::const_iterator;
    using iterator        = za::Conditional<is_map_v<T>, typename value_container_type::iterator, const_iterator>;
    using bucket_type     = Bucket;

private:
    using value_idx_type            = decltype(Bucket::m_value_idx);
    using dist_and_fingerprint_type = decltype(Bucket::m_dist_and_fingerprint);

    static_assert(ZA_IS_TRIVIALLY_DESTRUCTIBLE(Bucket),
                  "assert there's no need to call destructor / std::destroy");
    static_assert(ZA_IS_TRIVIALLY_COPYABLE(Bucket), "assert we can just memset / memcpy");

    value_container_type  m_values{}; // Contains all the key-value pairs in one densely stored container. No holes.
    bucket_container_type m_buckets{};
    za::SizeT       m_max_bucket_capacity = 0;
    za::SizeT       m_bucket_idx_mask     = 0; // bucket_count - 1; bucket_count is always a power of 2
    float                 m_max_load_factor     = default_max_load_factor;
    Hash                  m_hash{};
    KeyEqual              m_equal{};
    za::U8          m_shifts = initial_shifts;

    // Branchless wraparound using `bucket_count - 1` as a mask
    // (bucket counts are always powers of two -- see `calc_num_buckets`).
    [[nodiscard, gnu::always_inline]] auto next(value_idx_type bucket_idx) const -> value_idx_type
    {
        return static_cast<value_idx_type>((bucket_idx + 1U) & m_bucket_idx_mask);
    }

    // Helper to access bucket through pointer types
    [[nodiscard, gnu::always_inline]] static constexpr auto at(bucket_container_type& bucket, za::SizeT offset) -> Bucket&
    {
        return bucket[offset];
    }

    [[nodiscard, gnu::always_inline]] static constexpr auto at(const bucket_container_type& bucket, za::SizeT offset) -> const Bucket&
    {
        return bucket[offset];
    }

    // use the dist_inc and dist_dec functions so that uint16_t types work without warning
    [[nodiscard, gnu::always_inline]] static constexpr auto dist_inc(dist_and_fingerprint_type x) -> dist_and_fingerprint_type
    {
        return static_cast<dist_and_fingerprint_type>(x + Bucket::dist_inc);
    }

    [[nodiscard, gnu::always_inline]] static constexpr auto dist_dec(dist_and_fingerprint_type x) -> dist_and_fingerprint_type
    {
        return static_cast<dist_and_fingerprint_type>(x - Bucket::dist_inc);
    }

    // The goal of mixed_hash is to always produce a high quality 64bit hash.
    template <typename K>
    [[nodiscard, gnu::pure, gnu::always_inline]] constexpr auto mixed_hash(const K& key) const -> za::U64
    {
        if constexpr (requires { typename Hash::is_avalanching; })
        {
            // we know that the hash is good because is_avalanching.
            if constexpr (sizeof(decltype(m_hash(key))) < sizeof(za::U64))
            {
                // 32bit hash and is_avalanching => multiply with a constant to avalanche bits upwards
                return m_hash(key) * za::U64(0x9d'df'ea'08'eb'38'2d'69);
            }
            else
            {
                // 64bit and is_avalanching => only use the hash itself.
                return m_hash(key);
            }
        }
        else
        {
            // not is_avalanching => apply wyhash
            return wyhash::hash(m_hash(key));
        }
    }

    [[nodiscard, gnu::always_inline]] constexpr auto dist_and_fingerprint_from_hash(za::U64 hash) const -> dist_and_fingerprint_type
    {
        return Bucket::dist_inc | (static_cast<dist_and_fingerprint_type>(hash) & Bucket::fingerprint_mask);
    }

    [[nodiscard, gnu::always_inline]] constexpr auto bucket_idx_from_hash(za::U64 hash) const -> value_idx_type
    {
        return static_cast<value_idx_type>(hash >> m_shifts);
    }

    [[nodiscard, gnu::always_inline]] static constexpr auto get_key(const value_type& vt) -> const key_type&
    {
        if constexpr (is_map_v<T>)
        {
            return vt.first;
        }
        else
        {
            return vt;
        }
    }

    template <typename K>
    [[nodiscard, gnu::always_inline]] auto next_while_less(const K& key) const -> Bucket
    {
        auto hash                 = mixed_hash(key);
        auto dist_and_fingerprint = dist_and_fingerprint_from_hash(hash);
        auto bucket_idx           = bucket_idx_from_hash(hash);

        while (dist_and_fingerprint < at(m_buckets, bucket_idx).m_dist_and_fingerprint)
        {
            dist_and_fingerprint = dist_inc(dist_and_fingerprint);
            bucket_idx           = next(bucket_idx);
        }
        return {dist_and_fingerprint, bucket_idx};
    }

    void place_and_shift_up(Bucket bucket, value_idx_type place)
    {
        while (0 != at(m_buckets, place).m_dist_and_fingerprint)
        {
            bucket                        = za::exchange(at(m_buckets, place), bucket);
            bucket.m_dist_and_fingerprint = dist_inc(bucket.m_dist_and_fingerprint);
            place                         = next(place);
        }
        at(m_buckets, place) = bucket;
    }

    // Robin-Hood backward-shift starting at `bucket_idx`. After this call,
    // `bucket_idx` is either empty or contains an element with its preferred
    // displacement. Used by both `do_erase` and `replace_key`.
    void erase_and_shift_down(value_idx_type bucket_idx)
    {
        auto next_bucket_idx = next(bucket_idx);
        while (at(m_buckets, next_bucket_idx).m_dist_and_fingerprint >= Bucket::dist_inc * 2)
        {
            auto& next_bucket         = at(m_buckets, next_bucket_idx);
            at(m_buckets, bucket_idx) = {dist_dec(next_bucket.m_dist_and_fingerprint), next_bucket.m_value_idx};
            bucket_idx                = za::exchange(next_bucket_idx, next(next_bucket_idx));
        }
        at(m_buckets, bucket_idx) = {};
    }

    [[nodiscard]] static constexpr auto calc_num_buckets(za::U8 shifts) -> za::SizeT
    {
        return (za::min)(max_bucket_count(), za::SizeT{1} << (64U - shifts));
    }

    [[nodiscard]] constexpr auto calc_shifts_for_size(za::SizeT s) const -> za::U8
    {
        auto shifts = initial_shifts;
        while (shifts > 0 &&
               static_cast<za::SizeT>(static_cast<float>(calc_num_buckets(shifts)) * max_load_factor()) < s)
        {
            --shifts;
        }
        return shifts;
    }

    // assumes m_values has data, m_buckets=m_buckets_end=nullptr, m_shifts is INITIAL_SHIFTS
    void copy_buckets(const table& other)
    {
        // assumes m_values has already the correct data copied over.
        if (empty())
        {
            // when empty, at least allocate an initial buckets and clear them.
            allocate_buckets_from_shift();
            clear_buckets();
        }
        else
        {
            m_shifts = other.m_shifts;
            allocate_buckets_from_shift();
            if constexpr (IsSegmented || !ZA_IS_SAME(BucketContainer, default_container_t))
            {
                for (auto i = 0UL; i < bucket_count(); ++i)
                {
                    at(m_buckets, i) = at(other.m_buckets, i);
                }
            }
            else
            {
                ZA_MEMCPY(m_buckets.data(), other.m_buckets.data(), sizeof(Bucket) * bucket_count());
            }
        }
    }

    /**
     * True when no element can be added any more without increasing the size
     */
    [[nodiscard, gnu::always_inline]] auto is_full() const -> bool
    {
        return size() > m_max_bucket_capacity;
    }

    void deallocate_buckets()
    {
        m_buckets.clear();
        m_buckets.shrinkToFit();
        m_max_bucket_capacity = 0;
        m_bucket_idx_mask     = 0;
    }

    void allocate_buckets_from_shift()
    {
        auto num_buckets = calc_num_buckets(m_shifts);
        if constexpr (IsSegmented || !ZA_IS_SAME(BucketContainer, default_container_t))
        {
            if constexpr (has_reserve<bucket_container_type>)
            {
                m_buckets.reserve(num_buckets);
            }
            for (za::SizeT i = m_buckets.size(); i < num_buckets; ++i)
            {
                m_buckets.emplaceBack();
            }
        }
        else
        {
            // Left uninitialized on purpose: every caller immediately clears (`clear_buckets`,
            // `clear_and_fill_buckets_from_values`) or overwrites (`copy_buckets`) all buckets.
            m_buckets.reserve(num_buckets);
            m_buckets.unsafeSetSize(num_buckets);
        }
        if (num_buckets == max_bucket_count())
        {
            // reached the maximum, make sure we can use each bucket
            m_max_bucket_capacity = max_bucket_count();
        }
        else
        {
            m_max_bucket_capacity = static_cast<value_idx_type>(static_cast<float>(num_buckets) * max_load_factor());
        }
        m_bucket_idx_mask = num_buckets - 1U; // num_buckets is always a power of two
    }

    void clear_buckets()
    {
        if constexpr (IsSegmented || !ZA_IS_SAME(BucketContainer, default_container_t))
        {
            for (auto&& e : m_buckets)
            {
                ZA_MEMSET(&e, 0, sizeof(e));
            }
        }
        else
        {
            ZA_MEMSET(m_buckets.data(), 0, sizeof(Bucket) * bucket_count());
        }
    }

    void clear_and_fill_buckets_from_values()
    {
        clear_buckets();
        for (value_idx_type value_idx = 0, end_idx = static_cast<value_idx_type>(m_values.size()); value_idx < end_idx;
             ++value_idx)
        {
            const auto& key                     = get_key(m_values[value_idx]);
            auto [dist_and_fingerprint, bucket] = next_while_less(key);

            // we know for certain that key has not yet been inserted, so no need to check it.
            place_and_shift_up({dist_and_fingerprint, value_idx}, bucket);
        }
    }

    void increase_size()
    {
        if (m_max_bucket_capacity == max_bucket_count())
        {
            // remove the value again, we can't add it!
            m_values.popBack();
            za::abort(); // on_error_bucket_overflow();
        }
        --m_shifts;
        if constexpr (!IsSegmented || ZA_IS_SAME(BucketContainer, default_container_t))
        {
            deallocate_buckets();
        }
        allocate_buckets_from_shift();
        clear_and_fill_buckets_from_values();
    }

    template <typename Op>
    void do_erase(value_idx_type bucket_idx, Op handle_erased_value)
    {
        const auto value_idx_to_remove = at(m_buckets, bucket_idx).m_value_idx;
        erase_and_shift_down(bucket_idx);
        handle_erased_value(ZA_MOVE(m_values[value_idx_to_remove]));

        // update m_values
        if (value_idx_to_remove != m_values.size() - 1)
        {
            // no luck, we'll have to replace the value with the last one and update the index accordingly
            auto& val = m_values[value_idx_to_remove];
            val       = ZA_MOVE(m_values.back());

            // update the values_idx of the moved entry. No need to play the info game, just look until we find the values_idx
            bucket_idx = bucket_idx_from_hash(mixed_hash(get_key(val)));

            const auto values_idx_back = static_cast<value_idx_type>(m_values.size() - 1);
            while (values_idx_back != at(m_buckets, bucket_idx).m_value_idx)
            {
                bucket_idx = next(bucket_idx);
            }
            at(m_buckets, bucket_idx).m_value_idx = value_idx_to_remove;
        }
        m_values.popBack();
    }

    template <typename K, typename Op>
    auto do_erase_key(K&& key, Op handle_erased_value) -> za::SizeT
    {
        if (empty())
        {
            return 0;
        }

        auto [dist_and_fingerprint, bucket_idx] = next_while_less(key);

        while (dist_and_fingerprint == at(m_buckets, bucket_idx).m_dist_and_fingerprint &&
               !m_equal(key, get_key(m_values[at(m_buckets, bucket_idx).m_value_idx])))
        {
            dist_and_fingerprint = dist_inc(dist_and_fingerprint);
            bucket_idx           = next(bucket_idx);
        }

        if (dist_and_fingerprint != at(m_buckets, bucket_idx).m_dist_and_fingerprint)
        {
            return 0;
        }
        do_erase(bucket_idx, handle_erased_value);
        return 1;
    }

    template <class K, class M>
    auto do_insert_or_assign(K&& key, M&& mapped) -> detail::pair<iterator, bool>
    {
        auto it_isinserted = try_emplace(ZA_FORWARD(key), ZA_FORWARD(mapped));
        if (!it_isinserted.second)
        {
            it_isinserted.first->second = ZA_FORWARD(mapped);
        }
        return it_isinserted;
    }

    template <typename... Args>
    auto do_place_element(dist_and_fingerprint_type dist_and_fingerprint, value_idx_type bucket_idx, Args&&... args)
        -> detail::pair<iterator, bool>
    {
        // emplace the new value. If that throws an exception, no harm done; index is still in a valid state
        m_values.emplaceBack(ZA_FORWARD(args)...);

        auto value_idx = static_cast<value_idx_type>(m_values.size() - 1);
        if (is_full()) [[unlikely]]
        {
            increase_size();
        }
        else
        {
            place_and_shift_up({dist_and_fingerprint, value_idx}, bucket_idx);
        }

        // place element and shift up until we find an empty spot
        return {begin() + static_cast<difference_type>(value_idx), true};
    }

    template <typename K, typename... Args>
    auto do_try_emplace(K&& key, Args&&... args) -> detail::pair<iterator, bool>
    {
        auto hash                 = mixed_hash(key);
        auto dist_and_fingerprint = dist_and_fingerprint_from_hash(hash);
        auto bucket_idx           = bucket_idx_from_hash(hash);

        while (true)
        {
            auto* bucket = &at(m_buckets, bucket_idx);
            if (dist_and_fingerprint == bucket->m_dist_and_fingerprint)
            {
                if (m_equal(key, get_key(m_values[bucket->m_value_idx])))
                {
                    return {begin() + static_cast<difference_type>(bucket->m_value_idx), false};
                }
            }
            else if (dist_and_fingerprint > bucket->m_dist_and_fingerprint)
            {
                return do_place_element(dist_and_fingerprint,
                                        bucket_idx,
                                        detail::piecewise_fn{}, //
                                        ZA_FORWARD(key),
                                        ZA_FORWARD(args)...);
            }
            dist_and_fingerprint = dist_inc(dist_and_fingerprint);
            bucket_idx           = next(bucket_idx);
        }
    }

    template <typename K>
    auto do_find(const K& key) -> iterator
    {
        if (empty()) [[unlikely]]
        {
            return end();
        }

        auto  mh                   = mixed_hash(key);
        auto  dist_and_fingerprint = dist_and_fingerprint_from_hash(mh);
        auto  bucket_idx           = bucket_idx_from_hash(mh);
        auto* bucket               = &at(m_buckets, bucket_idx);

        // unrolled loop. *Always* check a few directly, then enter the loop. This is faster.
        if (dist_and_fingerprint == bucket->m_dist_and_fingerprint && m_equal(key, get_key(m_values[bucket->m_value_idx])))
        {
            return begin() + static_cast<difference_type>(bucket->m_value_idx);
        }
        dist_and_fingerprint = dist_inc(dist_and_fingerprint);
        bucket_idx           = next(bucket_idx);
        bucket               = &at(m_buckets, bucket_idx);

        if (dist_and_fingerprint == bucket->m_dist_and_fingerprint && m_equal(key, get_key(m_values[bucket->m_value_idx])))
        {
            return begin() + static_cast<difference_type>(bucket->m_value_idx);
        }
        dist_and_fingerprint = dist_inc(dist_and_fingerprint);
        bucket_idx           = next(bucket_idx);
        bucket               = &at(m_buckets, bucket_idx);

        while (true)
        {
            if (dist_and_fingerprint == bucket->m_dist_and_fingerprint)
            {
                if (m_equal(key, get_key(m_values[bucket->m_value_idx])))
                {
                    return begin() + static_cast<difference_type>(bucket->m_value_idx);
                }
            }
            else if (dist_and_fingerprint > bucket->m_dist_and_fingerprint)
            {
                return end();
            }
            dist_and_fingerprint = dist_inc(dist_and_fingerprint);
            bucket_idx           = next(bucket_idx);
            bucket               = &at(m_buckets, bucket_idx);
        }
    }

    template <typename K>
    [[gnu::always_inline]] auto do_find(const K& key) const -> const_iterator
    {
        return const_cast<table*>(this)->do_find(key); // NOLINT(cppcoreguidelines-pro-type-const-cast)
    }

    template <typename K, typename Q = T>
    auto do_at(const K& key) -> Q&
        requires(is_map_v<Q>)
    {
        if (auto it = find(key); end() != it) [[likely]]
        {
            return it->second;
        }
        za::abort(); // on_error_key_not_found();
    }

    template <typename K, typename Q = T>
    auto do_at(const K& key) const -> const Q&
        requires(is_map_v<Q>)
    {
        return const_cast<table*>(this)->at(key); // NOLINT(cppcoreguidelines-pro-type-const-cast)
    }

public:
    table() : table(0u)
    {
    }

    explicit table(za::SizeT bucket_count, const Hash& hash = Hash(), const KeyEqual& equal = KeyEqual()) :
        m_values(),
        m_buckets(),
        m_hash(hash),
        m_equal(equal)
    {
        if (0 != bucket_count)
        {
            reserve(bucket_count);
        }
        else
        {
            allocate_buckets_from_shift();
            clear_buckets();
        }
    }

    template <class InputIt>
    table(InputIt         first,
          InputIt         last,
          size_type       bucket_count = 0,
          const Hash&     hash         = Hash(),
          const KeyEqual& equal        = KeyEqual()) :
        table(bucket_count, hash, equal)
    {
        insert(first, last);
    }

    table(const table& other) = default;

    // Like upstream, the moved-from table is left equivalent to a default-constructed one (usable, with buckets).
    table(table&& other) noexcept :
        m_values(ZA_MOVE(other.m_values)),
        m_buckets(ZA_MOVE(other.m_buckets)),
        m_max_bucket_capacity(za::exchange(other.m_max_bucket_capacity, za::SizeT{0})),
        m_bucket_idx_mask(za::exchange(other.m_bucket_idx_mask, za::SizeT{0})),
        m_max_load_factor(za::exchange(other.m_max_load_factor, default_max_load_factor)),
        m_hash(za::exchange(other.m_hash, {})),
        m_equal(za::exchange(other.m_equal, {})),
        m_shifts(za::exchange(other.m_shifts, initial_shifts))
    {
        other.allocate_buckets_from_shift();
        other.clear_buckets();
    }

    table(std::initializer_list<value_type> ilist,
          za::SizeT                   bucket_count = 0,
          const Hash&                       hash         = Hash(),
          const KeyEqual&                   equal        = KeyEqual()) :
        table(bucket_count, hash, equal)
    {
        insert(ilist);
    }

    table(std::initializer_list<value_type> ilist, size_type bucket_count) :
        table(ilist, bucket_count, Hash(), KeyEqual())
    {
    }

    table(std::initializer_list<value_type> init, size_type bucket_count, const Hash& hash) :
        table(init, bucket_count, hash, KeyEqual())
    {
    }

    ~table() = default;

    auto operator=(const table& other) -> table&
    {
        if (&other != this)
        {
            m_values          = other.m_values;
            m_max_load_factor = other.m_max_load_factor;
            m_hash            = other.m_hash;
            m_equal           = other.m_equal;
            m_shifts          = initial_shifts;
            copy_buckets(other);
        }
        return *this;
    }

    auto operator=(table&& other) noexcept(ZA_IS_NOTHROW_MOVE_ASSIGNABLE(value_container_type) &&
                                           ZA_IS_NOTHROW_MOVE_ASSIGNABLE(Hash) &&
                                           ZA_IS_NOTHROW_MOVE_ASSIGNABLE(KeyEqual)) -> table&
    {
        if (&other != this)
        {
            m_values              = ZA_MOVE(other.m_values);
            m_buckets             = ZA_MOVE(other.m_buckets);
            m_max_bucket_capacity = za::exchange(other.m_max_bucket_capacity, za::SizeT{0});
            m_bucket_idx_mask     = za::exchange(other.m_bucket_idx_mask, za::SizeT{0});
            m_shifts              = za::exchange(other.m_shifts, initial_shifts);
            m_max_load_factor     = za::exchange(other.m_max_load_factor, default_max_load_factor);
            m_hash                = za::exchange(other.m_hash, {});
            m_equal               = za::exchange(other.m_equal, {});

            // leave "other" usable and empty, like a default-constructed table
            other.allocate_buckets_from_shift();
            other.clear_buckets();
        }
        return *this;
    }

    auto operator=(std::initializer_list<value_type> ilist) -> table&
    {
        clear();
        insert(ilist);
        return *this;
    }

    // iterators ////////////////////////////////////////////////////////////

    [[gnu::always_inline]] auto begin() noexcept -> iterator
    {
        return m_values.begin();
    }

    [[gnu::always_inline]] auto begin() const noexcept -> const_iterator
    {
        return m_values.begin();
    }

    [[gnu::always_inline]] auto cbegin() const noexcept -> const_iterator
    {
        return m_values.cbegin();
    }

    [[gnu::always_inline]] auto end() noexcept -> iterator
    {
        return m_values.end();
    }

    [[gnu::always_inline]] auto cend() const noexcept -> const_iterator
    {
        return m_values.cend();
    }

    [[gnu::always_inline]] auto end() const noexcept -> const_iterator
    {
        return m_values.end();
    }

    // capacity /////////////////////////////////////////////////////////////

    [[nodiscard, gnu::always_inline]] auto empty() const noexcept -> bool
    {
        return m_values.empty();
    }

    [[nodiscard, gnu::always_inline]] auto size() const noexcept -> za::SizeT
    {
        return m_values.size();
    }

    [[nodiscard]] static constexpr auto max_size() noexcept -> za::SizeT
    {
        if constexpr (value_idx_type(-1) == za::SizeT(-1))
        {
            return za::SizeT{1} << (sizeof(value_idx_type) * 8 - 1);
        }
        else
        {
            return za::SizeT{1} << (sizeof(value_idx_type) * 8);
        }
    }

    // modifiers ////////////////////////////////////////////////////////////

    void clear()
    {
        m_values.clear();
        clear_buckets();
    }

    auto insert(const value_type& value) -> detail::pair<iterator, bool>
    {
        return emplace(value);
    }

    auto insert(value_type&& value) -> detail::pair<iterator, bool>
    {
        return emplace(ZA_MOVE(value));
    }

    template <class P>
    auto insert(P&& value) -> detail::pair<iterator, bool>
        requires(za::isConstructible<value_type, P &&>)
    {
        return emplace(ZA_FORWARD(value));
    }

    auto insert(const_iterator /*hint*/, const value_type& value) -> iterator
    {
        return insert(value).first;
    }

    auto insert(const_iterator /*hint*/, value_type&& value) -> iterator
    {
        return insert(ZA_MOVE(value)).first;
    }

    template <class P>
    auto insert(const_iterator /*hint*/, P&& value) -> iterator
        requires(za::isConstructible<value_type, P &&>)
    {
        return insert(ZA_FORWARD(value)).first;
    }

    template <class InputIt>
    void insert(InputIt first, InputIt last)
    {
        while (first != last)
        {
            insert(*first);
            ++first;
        }
    }

    void insert(std::initializer_list<value_type> ilist)
    {
        insert(ilist.begin(), ilist.end());
    }

    // nonstandard API: *this is emptied.
    // Also see "A Standard flat_map" https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2022/p0429r9.pdf
    auto extract() && -> value_container_type
    {
        return ZA_MOVE(m_values);
    }

    // nonstandard API:
    // Discards the internally held container and replaces it with the one passed. Erases non-unique elements.
    auto replace(value_container_type&& container)
    {
        if (container.size() > max_size()) [[unlikely]]
        {
            za::abort(); // on_error_too_many_elements();
        }
        auto shifts = calc_shifts_for_size(container.size());
        if (0 == bucket_count() || shifts < m_shifts)
        {
            m_shifts = shifts;
            deallocate_buckets();
            allocate_buckets_from_shift();
        }
        clear_buckets();

        m_values = ZA_MOVE(container);

        // can't use clear_and_fill_buckets_from_values() because container elements might not be unique
        auto value_idx = value_idx_type{};

        // loop until we reach the end of the container. duplicated entries will be replaced with back().
        while (value_idx != static_cast<value_idx_type>(m_values.size()))
        {
            const auto& key = get_key(m_values[value_idx]);

            auto hash                 = mixed_hash(key);
            auto dist_and_fingerprint = dist_and_fingerprint_from_hash(hash);
            auto bucket_idx           = bucket_idx_from_hash(hash);

            bool key_found = false;
            while (true)
            {
                const auto& bucket = at(m_buckets, bucket_idx);
                if (dist_and_fingerprint > bucket.m_dist_and_fingerprint)
                {
                    break;
                }
                if (dist_and_fingerprint == bucket.m_dist_and_fingerprint &&
                    m_equal(key, get_key(m_values[bucket.m_value_idx])))
                {
                    key_found = true;
                    break;
                }
                dist_and_fingerprint = dist_inc(dist_and_fingerprint);
                bucket_idx           = next(bucket_idx);
            }

            if (key_found)
            {
                if (value_idx != static_cast<value_idx_type>(m_values.size() - 1))
                {
                    m_values[value_idx] = ZA_MOVE(m_values.back());
                }
                m_values.popBack();
            }
            else
            {
                place_and_shift_up({dist_and_fingerprint, value_idx}, bucket_idx);
                ++value_idx;
            }
        }
    }

    template <class M, typename Q = T>
    auto insert_or_assign(const Key& key, M&& mapped) -> detail::pair<iterator, bool>
        requires(is_map_v<Q> && za::isConstructible<Q, M &&>)
    {
        return do_insert_or_assign(key, ZA_FORWARD(mapped));
    }

    template <class M, typename Q = T>
    auto insert_or_assign(Key&& key, M&& mapped) -> detail::pair<iterator, bool>
        requires(is_map_v<Q> && za::isConstructible<Q, M &&>)
    {
        return do_insert_or_assign(ZA_MOVE(key), ZA_FORWARD(mapped));
    }

    template <typename K, typename M, typename Q = T, typename H = Hash, typename KE = KeyEqual>
    auto insert_or_assign(K&& key, M&& mapped) -> detail::pair<iterator, bool>
        requires(is_map_v<Q> && is_transparent_v<H, KE> && za::isConstructible<Key, K &&> && za::isConstructible<Q, M &&>)
    {
        return do_insert_or_assign(ZA_FORWARD(key), ZA_FORWARD(mapped));
    }

    template <class M, typename Q = T>
    auto insert_or_assign(const_iterator /*hint*/, const Key& key, M&& mapped) -> iterator
        requires(is_map_v<Q> && za::isConstructible<Q, M &&>)
    {
        return do_insert_or_assign(key, ZA_FORWARD(mapped)).first;
    }

    template <class M, typename Q = T>
    auto insert_or_assign(const_iterator /*hint*/, Key&& key, M&& mapped) -> iterator
        requires(is_map_v<Q> && za::isConstructible<Q, M &&>)
    {
        return do_insert_or_assign(ZA_MOVE(key), ZA_FORWARD(mapped)).first;
    }

    template <typename K, typename M, typename Q = T, typename H = Hash, typename KE = KeyEqual>
    auto insert_or_assign(const_iterator /*hint*/, K&& key, M&& mapped) -> iterator
        requires(is_map_v<Q> && is_transparent_v<H, KE> && za::isConstructible<Key, K &&> && za::isConstructible<Q, M &&>)
    {
        return do_insert_or_assign(ZA_FORWARD(key), ZA_FORWARD(mapped)).first;
    }

    // Single arguments for unordered set can be used without having to construct the value_type
    template <class K, typename Q = T, typename H = Hash, typename KE = KeyEqual>
    auto emplace(K&& key) -> detail::pair<iterator, bool>
        requires(!is_map_v<Q> && is_transparent_v<H, KE>)
    {
        auto hash                 = mixed_hash(key);
        auto dist_and_fingerprint = dist_and_fingerprint_from_hash(hash);
        auto bucket_idx           = bucket_idx_from_hash(hash);

        while (dist_and_fingerprint <= at(m_buckets, bucket_idx).m_dist_and_fingerprint)
        {
            if (dist_and_fingerprint == at(m_buckets, bucket_idx).m_dist_and_fingerprint &&
                m_equal(key, m_values[at(m_buckets, bucket_idx).m_value_idx]))
            {
                // found it, return without ever actually creating anything
                return {begin() + static_cast<difference_type>(at(m_buckets, bucket_idx).m_value_idx), false};
            }
            dist_and_fingerprint = dist_inc(dist_and_fingerprint);
            bucket_idx           = next(bucket_idx);
        }

        // value is new, insert element first, so when exception happens we are in a valid state
        return do_place_element(dist_and_fingerprint, bucket_idx, ZA_FORWARD(key));
    }

    template <class... Args>
    auto emplace(Args&&... args) -> detail::pair<iterator, bool>
    {
        // we have to instantiate the value_type to be able to access the key.
        // 1. emplace_back the object so it is constructed. 2. If the key is already there, pop it later in the loop.
        auto& key                  = get_key(m_values.emplaceBack(ZA_FORWARD(args)...));
        auto  hash                 = mixed_hash(key);
        auto  dist_and_fingerprint = dist_and_fingerprint_from_hash(hash);
        auto  bucket_idx           = bucket_idx_from_hash(hash);

        while (dist_and_fingerprint <= at(m_buckets, bucket_idx).m_dist_and_fingerprint)
        {
            if (dist_and_fingerprint == at(m_buckets, bucket_idx).m_dist_and_fingerprint &&
                m_equal(key, get_key(m_values[at(m_buckets, bucket_idx).m_value_idx])))
            {
                m_values.popBack(); // value was already there, so get rid of it
                return {begin() + static_cast<difference_type>(at(m_buckets, bucket_idx).m_value_idx), false};
            }
            dist_and_fingerprint = dist_inc(dist_and_fingerprint);
            bucket_idx           = next(bucket_idx);
        }

        // value is new, place the bucket and shift up until we find an empty spot
        auto value_idx = static_cast<value_idx_type>(m_values.size() - 1);
        if (is_full()) [[unlikely]]
        {
            // increase_size just rehashes all the data we have in m_values
            increase_size();
        }
        else
        {
            // place element and shift up until we find an empty spot
            place_and_shift_up({dist_and_fingerprint, value_idx}, bucket_idx);
        }
        return {begin() + static_cast<difference_type>(value_idx), true};
    }

    template <class... Args>
    auto emplace_hint(const_iterator /*hint*/, Args&&... args) -> iterator
    {
        return emplace(ZA_FORWARD(args)...).first;
    }

    template <class... Args, typename Q = T>
    [[gnu::always_inline]] auto try_emplace(const Key& key, Args&&... args) -> detail::pair<iterator, bool>
        requires(is_map_v<Q> && za::isConstructible<Q, Args &&...>)
    {
        return do_try_emplace(key, ZA_FORWARD(args)...);
    }

    template <class... Args, typename Q = T>
    [[gnu::always_inline]] auto try_emplace(Key&& key, Args&&... args) -> detail::pair<iterator, bool>
        requires(is_map_v<Q> && za::isConstructible<Q, Args &&...>)
    {
        return do_try_emplace(ZA_MOVE(key), ZA_FORWARD(args)...);
    }

    template <class... Args, typename Q = T>
    auto try_emplace(const_iterator /*hint*/, const Key& key, Args&&... args) -> iterator
        requires(is_map_v<Q> && za::isConstructible<Q, Args &&...>)
    {
        return do_try_emplace(key, ZA_FORWARD(args)...).first;
    }

    template <class... Args, typename Q = T>
    auto try_emplace(const_iterator /*hint*/, Key&& key, Args&&... args) -> iterator
        requires(is_map_v<Q> && za::isConstructible<Q, Args &&...>)
    {
        return do_try_emplace(ZA_MOVE(key), ZA_FORWARD(args)...).first;
    }

    template <typename K, typename... Args, typename Q = T, typename H = Hash, typename KE = KeyEqual>
    [[gnu::always_inline]] auto try_emplace(K&& key, Args&&... args) -> detail::pair<iterator, bool>
        requires(is_map_v<Q> && is_transparent_v<H, KE> && is_neither_convertible_v<K &&, iterator, const_iterator> &&
                 za::isConstructible<Key, K &&> && za::isConstructible<Q, Args &&...>)
    {
        return do_try_emplace(ZA_FORWARD(key), ZA_FORWARD(args)...);
    }

    template <typename K, typename... Args, typename Q = T, typename H = Hash, typename KE = KeyEqual>
    auto try_emplace(const_iterator /*hint*/, K&& key, Args&&... args) -> iterator
        requires(is_map_v<Q> && is_transparent_v<H, KE> && is_neither_convertible_v<K &&, iterator, const_iterator> &&
                 za::isConstructible<Key, K &&> && za::isConstructible<Q, Args &&...>)
    {
        return do_try_emplace(ZA_FORWARD(key), ZA_FORWARD(args)...).first;
    }

    // Replaces the key at `it` with `new_key` without invalidating iterators
    // or references. Returns `{iterator, false}` pointing at the existing
    // `new_key` (and makes no change) if `new_key` is already present in the
    // table; otherwise `{it, true}`.
    //
    // Cheaper than erase+insert because the value never moves: only the
    // key is replaced in place and the buckets are rewired.
    template <typename K>
    auto replace_key(iterator it, K&& new_key) -> detail::pair<iterator, bool>
    {
        const auto new_key_hash = mixed_hash(new_key);

        // First, see if `new_key` already exists in the table.
        auto dist_and_fingerprint = dist_and_fingerprint_from_hash(new_key_hash);
        auto bucket_idx           = bucket_idx_from_hash(new_key_hash);
        while (dist_and_fingerprint <= at(m_buckets, bucket_idx).m_dist_and_fingerprint)
        {
            const auto& bucket = at(m_buckets, bucket_idx);
            if (dist_and_fingerprint == bucket.m_dist_and_fingerprint &&
                m_equal(new_key, get_key(m_values[bucket.m_value_idx])))
            {
                return {begin() + static_cast<difference_type>(bucket.m_value_idx), false};
            }
            dist_and_fingerprint = dist_inc(dist_and_fingerprint);
            bucket_idx           = next(bucket_idx);
        }

        // `const_cast` because set iterators are always `const`: we can't
        // add another `get_key` overload returning a mutable reference
        // without complicating the value-type discrimination.
        auto&      target_key         = const_cast<key_type&>(get_key(*it));
        const auto old_key_bucket_idx = bucket_idx_from_hash(mixed_hash(target_key));

        // Replace the key *before* touching the buckets. If the assignment
        // throws, the table state is still consistent (the buckets still
        // index the unchanged value at the same position).
        target_key = ZA_FORWARD(new_key);

        const auto value_idx = static_cast<value_idx_type>(it - begin());

        // Find the bucket owning our `value_idx`. Guaranteed to exist.
        bucket_idx = old_key_bucket_idx;
        while (value_idx != at(m_buckets, bucket_idx).m_value_idx)
        {
            bucket_idx = next(bucket_idx);
        }
        erase_and_shift_down(bucket_idx);

        // Place the new bucket at the natural position for `new_key_hash`.
        dist_and_fingerprint = dist_and_fingerprint_from_hash(new_key_hash);
        bucket_idx           = bucket_idx_from_hash(new_key_hash);
        while (dist_and_fingerprint < at(m_buckets, bucket_idx).m_dist_and_fingerprint)
        {
            dist_and_fingerprint = dist_inc(dist_and_fingerprint);
            bucket_idx           = next(bucket_idx);
        }
        place_and_shift_up({dist_and_fingerprint, value_idx}, bucket_idx);

        return {it, true};
    }

    auto erase(iterator it) -> iterator
    {
        auto hash       = mixed_hash(get_key(*it));
        auto bucket_idx = bucket_idx_from_hash(hash);

        const auto value_idx_to_remove = static_cast<value_idx_type>(it - cbegin());
        while (at(m_buckets, bucket_idx).m_value_idx != value_idx_to_remove)
        {
            bucket_idx = next(bucket_idx);
        }

        do_erase(bucket_idx, [](value_type&& /*unused*/) {});
        return begin() + static_cast<difference_type>(value_idx_to_remove);
    }

    auto extract(iterator it) -> value_type
    {
        auto hash       = mixed_hash(get_key(*it));
        auto bucket_idx = bucket_idx_from_hash(hash);

        const auto value_idx_to_remove = static_cast<value_idx_type>(it - cbegin());
        while (at(m_buckets, bucket_idx).m_value_idx != value_idx_to_remove)
        {
            bucket_idx = next(bucket_idx);
        }

        auto tmp = za::Optional<value_type>{};
        do_erase(bucket_idx, [&tmp](value_type&& val) { tmp.emplace(ZA_MOVE(val)); });
        return ZA_MOVE(tmp).value();
    }

    template <typename Q = T>
    auto erase(const_iterator it) -> iterator
        requires(is_map_v<Q>)
    {
        return erase(begin() + (it - cbegin()));
    }

    template <typename Q = T>
    auto extract(const_iterator it) -> value_type
        requires(is_map_v<Q>)
    {
        return extract(begin() + (it - cbegin()));
    }

    auto erase(const_iterator first, const_iterator last) -> iterator
    {
        const auto idx_first     = first - cbegin();
        const auto idx_last      = last - cbegin();
        const auto first_to_last = detail::my_distance(first, last);
        const auto last_to_end   = detail::my_distance(last, cend());

        // remove elements from left to right which moves elements from the end back
        const auto mid = idx_first + (za::min)(first_to_last, last_to_end);
        auto       idx = idx_first;
        while (idx != mid)
        {
            erase(begin() + idx);
            ++idx;
        }

        // all elements from the right are moved, now remove the last element until all done
        idx = idx_last;
        while (idx != mid)
        {
            --idx;
            erase(begin() + idx);
        }

        return begin() + idx_first;
    }

    [[gnu::always_inline]] auto erase(const Key& key) -> za::SizeT
    {
        return do_erase_key(key, [](value_type&& /*unused*/) {});
    }

    auto extract(const Key& key) -> za::Optional<value_type>
    {
        auto tmp = za::Optional<value_type>{};
        do_erase_key(key, [&tmp](value_type&& val) { tmp.emplace(ZA_MOVE(val)); });
        return tmp;
    }

    template <class K, class H = Hash, class KE = KeyEqual>
    auto erase(K&& key) -> za::SizeT
        requires(is_transparent_v<H, KE>)
    {
        return do_erase_key(ZA_FORWARD(key), [](value_type&& /*unused*/) {});
    }

    template <class K, class H = Hash, class KE = KeyEqual>
    auto extract(K&& key) -> za::Optional<value_type>
        requires(is_transparent_v<H, KE>)
    {
        auto tmp = za::Optional<value_type>{};
        do_erase_key(ZA_FORWARD(key), [&tmp](value_type&& val) { tmp.emplace(ZA_MOVE(val)); });
        return tmp;
    }

    void swap(table& other) noexcept(ZA_IS_NOTHROW_SWAPPABLE(value_container_type) &&
                                     ZA_IS_NOTHROW_SWAPPABLE(Hash) &&
                                     ZA_IS_NOTHROW_SWAPPABLE(KeyEqual))
    {
        za::genericSwap(m_values, other.m_values);
        za::genericSwap(m_buckets, other.m_buckets);
        za::genericSwap(m_max_bucket_capacity, other.m_max_bucket_capacity);
        za::genericSwap(m_bucket_idx_mask, other.m_bucket_idx_mask);
        za::genericSwap(m_max_load_factor, other.m_max_load_factor);
        za::genericSwap(m_hash, other.m_hash);
        za::genericSwap(m_equal, other.m_equal);
        za::genericSwap(m_shifts, other.m_shifts);
    }

    friend void swap(table& lhs, table& rhs) noexcept(noexcept(lhs.swap(rhs)))
    {
        lhs.swap(rhs);
    }

    // lookup ///////////////////////////////////////////////////////////////

    template <typename Q = T>
    auto at(const key_type& key) -> Q&
        requires(is_map_v<Q>)
    {
        return do_at(key);
    }

    template <typename K, typename Q = T, typename H = Hash, typename KE = KeyEqual>
    auto at(const K& key) -> Q&
        requires(is_map_v<Q> && is_transparent_v<H, KE>)
    {
        return do_at(key);
    }

    template <typename Q = T>
    auto at(const key_type& key) const -> const Q&
        requires(is_map_v<Q>)
    {
        return do_at(key);
    }

    template <typename K, typename Q = T, typename H = Hash, typename KE = KeyEqual>
    auto at(const K& key) const -> const Q&
        requires(is_map_v<Q> && is_transparent_v<H, KE>)
    {
        return do_at(key);
    }

    template <typename Q = T>
    [[gnu::always_inline]] auto operator[](const Key& key) -> Q&
        requires(is_map_v<Q>)
    {
        return try_emplace(key).first->second;
    }

    template <typename Q = T>
    [[gnu::always_inline]] auto operator[](Key&& key) -> Q&
        requires(is_map_v<Q>)
    {
        return try_emplace(ZA_MOVE(key)).first->second;
    }

    template <typename K, typename Q = T, typename H = Hash, typename KE = KeyEqual>
    [[gnu::always_inline]] auto operator[](K&& key) -> Q&
        requires(is_map_v<Q> && is_transparent_v<H, KE>)
    {
        return try_emplace(ZA_FORWARD(key)).first->second;
    }

    auto count(const Key& key) const -> za::SizeT
    {
        return find(key) == end() ? 0 : 1;
    }

    template <class K, class H = Hash, class KE = KeyEqual>
    auto count(const K& key) const -> za::SizeT
        requires(is_transparent_v<H, KE>)
    {
        return find(key) == end() ? 0 : 1;
    }

    [[gnu::always_inline]] auto find(const Key& key) -> iterator
    {
        return do_find(key);
    }

    [[gnu::always_inline]] auto find(const Key& key) const -> const_iterator
    {
        return do_find(key);
    }

    template <class K, class H = Hash, class KE = KeyEqual>
    [[gnu::always_inline]] auto find(const K& key) -> iterator
        requires(is_transparent_v<H, KE>)
    {
        return do_find(key);
    }

    template <class K, class H = Hash, class KE = KeyEqual>
    [[gnu::always_inline]] auto find(const K& key) const -> const_iterator
        requires(is_transparent_v<H, KE>)
    {
        return do_find(key);
    }

    [[gnu::always_inline]] auto contains(const Key& key) const -> bool
    {
        return find(key) != end();
    }

    template <class K, class H = Hash, class KE = KeyEqual>
    [[gnu::always_inline]] auto contains(const K& key) const -> bool
        requires(is_transparent_v<H, KE>)
    {
        return find(key) != end();
    }

    auto equal_range(const Key& key) -> detail::pair<iterator, iterator>
    {
        auto it = do_find(key);
        return {it, it == end() ? end() : it + 1};
    }

    auto equal_range(const Key& key) const -> detail::pair<const_iterator, const_iterator>
    {
        auto it = do_find(key);
        return {it, it == end() ? end() : it + 1};
    }

    template <class K, class H = Hash, class KE = KeyEqual>
    auto equal_range(const K& key) -> detail::pair<iterator, iterator>
        requires(is_transparent_v<H, KE>)
    {
        auto it = do_find(key);
        return {it, it == end() ? end() : it + 1};
    }

    template <class K, class H = Hash, class KE = KeyEqual>
    auto equal_range(const K& key) const -> detail::pair<const_iterator, const_iterator>
        requires(is_transparent_v<H, KE>)
    {
        auto it = do_find(key);
        return {it, it == end() ? end() : it + 1};
    }

    // bucket interface ///////////////////////////////////////////////////////

    [[nodiscard]] auto bucket_count() const noexcept -> za::SizeT
    { // NOLINT(modernize-use-nodiscard)
        return m_buckets.size();
    }

    static constexpr auto max_bucket_count() noexcept -> za::SizeT
    { // NOLINT(modernize-use-nodiscard)
        return max_size();
    }

    // hash policy ////////////////////////////////////////////////////////////

    [[nodiscard]] auto load_factor() const -> float
    {
        return bucket_count() ? static_cast<float>(size()) / static_cast<float>(bucket_count()) : 0.f;
    }

    [[nodiscard]] auto max_load_factor() const -> float
    {
        return m_max_load_factor;
    }

    void max_load_factor(float ml)
    {
        m_max_load_factor = ml;
        if (bucket_count() != max_bucket_count())
        {
            m_max_bucket_capacity = static_cast<value_idx_type>(static_cast<float>(bucket_count()) * max_load_factor());
        }
    }

    void rehash(za::SizeT count)
    {
        count       = (za::min)(count, max_size());
        auto shifts = calc_shifts_for_size((za::max)(count, size()));
        if (shifts != m_shifts)
        {
            m_shifts = shifts;
            deallocate_buckets();
            m_values.shrinkToFit();
            allocate_buckets_from_shift();
            clear_and_fill_buckets_from_values();
        }
    }

    void reserve(za::SizeT capa)
    {
        capa = (za::min)(capa, max_size());
        if constexpr (has_reserve<value_container_type>)
        {
            // std::deque doesn't have reserve(). Make sure we only call when available
            m_values.reserve(capa);
        }
        auto shifts = calc_shifts_for_size((za::max)(capa, size()));
        if (0 == bucket_count() || shifts < m_shifts)
        {
            m_shifts = shifts;
            deallocate_buckets();
            allocate_buckets_from_shift();
            clear_and_fill_buckets_from_values();
        }
    }

    // observers ////////////////////////////////////////////////////////////

    auto hash_function() const -> hasher
    {
        return m_hash;
    }

    auto key_eq() const -> key_equal
    {
        return m_equal;
    }

    // nonstandard API: expose the underlying values container
    [[nodiscard]] auto values() const noexcept -> const value_container_type&
    {
        return m_values;
    }

    // non-member functions ///////////////////////////////////////////////////

    friend auto operator==(const table& a, const table& b) -> bool
    {
        if (&a == &b)
        {
            return true;
        }
        if (a.size() != b.size())
        {
            return false;
        }
        for (const auto& b_entry : b)
        {
            auto it = a.find(get_key(b_entry));
            if constexpr (is_map_v<T>)
            {
                // map: check that key is here, then also check that value is the same
                if (a.end() == it || !(b_entry.second == it->second))
                {
                    return false;
                }
            }
            else
            {
                // set: only check that the key is here
                if (a.end() == it)
                {
                    return false;
                }
            }
        }
        return true;
    }

    // Erases all elements satisfying `pred`, returns the number of erased elements. Found via ADL:
    // `erase_if(map, pred)` (replaces upstream's `std::erase_if` overload, which is UB to declare).
    template <typename Pred>
    friend auto erase_if(table& map, Pred pred) -> za::SizeT
    {
        // going back to front because erase() invalidates the end iterator
        const auto old_size = map.size();
        auto       idx      = old_size;
        while (idx)
        {
            --idx;
            auto it = map.begin() + static_cast<difference_type>(idx);
            if (pred(*it))
            {
                map.erase(it);
            }
        }

        return old_size - map.size();
    }
};

} // namespace detail

template <class Key,
          class T,
          class Hash            = hash<Key>,
          class KeyEqual        = detail::EqualTo<Key>,
          class Bucket          = bucket_type::standard,
          class BucketContainer = detail::default_container_t>
using map = detail::table<Key, T, Hash, KeyEqual, Bucket, BucketContainer, false>;

template <class Key,
          class Hash            = hash<Key>,
          class KeyEqual        = detail::EqualTo<Key>,
          class Bucket          = bucket_type::standard,
          class BucketContainer = detail::default_container_t>
using set = detail::table<Key, void, Hash, KeyEqual, Bucket, BucketContainer, false>;

#if 0 // segmented containers are currently disabled, see `segmented_vector`
template <class Key,
          class T,
          class Hash            = hash<Key>,
          class KeyEqual        = detail::EqualTo<Key>,
          class Bucket          = bucket_type::standard,
          class BucketContainer = detail::default_container_t>
using segmented_map = detail::table<Key, T, Hash, KeyEqual, Bucket, BucketContainer, true>;

template <class Key,
          class Hash            = hash<Key>,
          class KeyEqual        = detail::EqualTo<Key>,
          class Bucket          = bucket_type::standard,
          class BucketContainer = detail::default_container_t>
using segmented_set = detail::table<Key, void, Hash, KeyEqual, Bucket, BucketContainer, true>;
#endif

} // namespace ankerl::unordered_dense::inline v4_8_1


// NOLINTEND(readability-identifier-naming)

// clang-format on
