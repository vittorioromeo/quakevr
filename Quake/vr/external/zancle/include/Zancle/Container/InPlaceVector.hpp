#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include "Zancle/Container/Priv/SwapUnequalRanges.hpp"
#include "Zancle/Container/Priv/VectorUtils.hpp"

#include "Zancle/Base/Abort.hpp"
#include "Zancle/Base/Assert.hpp"
#include "Zancle/Base/InitializerList.hpp" // IWYU pragma: keep
#include "Zancle/Base/LifetimeAttributes.hpp"
#include "Zancle/Base/PlacementNew.hpp"
#include "Zancle/Base/PtrDiffT.hpp"
#include "Zancle/Base/SizeT.hpp"

#include "Zancle/Trait/EnableTrivialRelocation.hpp"
#include "Zancle/Trait/IsTriviallyDestructible.hpp"
#include "Zancle/Trait/IsTriviallyRelocatable.hpp"


namespace za::priv
{
////////////////////////////////////////////////////////////
/// \brief Called when a checked `InPlaceVector` operation would exceed its capacity
///
/// Reports an assertion failure in debug builds, and aborts in all builds.
///
////////////////////////////////////////////////////////////
[[noreturn, gnu::cold, gnu::noinline]] inline void inPlaceVectorCapacityExceeded() noexcept
{
#ifdef ZA_DEBUG
    priv::assertFailure("InPlaceVector capacity exceeded", __FILE__, __LINE__);
#else
    za::abort();
#endif
}

} // namespace za::priv


namespace za
{
////////////////////////////////////////////////////////////
/// \brief Bounded vector backed entirely by inline storage (no heap allocation)
///
/// `InPlaceVector<T, N>` provides the usual `Vector` interface but
/// stores all elements in a fixed-size aligned buffer of capacity `N`.
/// It never allocates and `capacity()` always returns `N`. Attempting
/// to grow past `N` is a programming error: checked operations (e.g.
/// `pushBack`, `emplace`, `resize`, `reserve`) abort the program in all
/// builds, while `unsafe*` operations only assert in debug builds.
///
/// \note Although its members are declared `constexpr`, `InPlaceVector`
///       is not yet usable in constant evaluation: `data()` requires a
///       `reinterpret_cast` of the raw storage and elements are created
///       with placement new, neither of which is allowed in constant
///       expressions in C++23. Revisit in C++26 (constexpr placement new,
///       trivial unions).
///
/// Useful when the maximum element count is known statically and you
/// want to avoid both heap traffic and the size overhead of
/// `SmallVector`'s heap escape hatch.
///
////////////////////////////////////////////////////////////
template <typename TItem, SizeT N>
class [[nodiscard]] ZA_GSL_OWNER(TItem) InPlaceVector // NOLINT(cppcoreguidelines-pro-type-member-init)
{
    static_assert(N > 0);

private:
    ////////////////////////////////////////////////////////////
    alignas(TItem) unsigned char m_storage[sizeof(TItem) * N];
    SizeT m_size{0u};


public:
    ////////////////////////////////////////////////////////////
    ZA_ENABLE_TRIVIAL_RELOCATION_IF(ZA_IS_TRIVIALLY_RELOCATABLE(TItem));


    ////////////////////////////////////////////////////////////
    using value_type      = TItem;
    using pointer         = TItem*;
    using const_pointer   = const TItem*;
    using reference       = TItem&;
    using const_reference = const TItem&;
    using size_type       = SizeT;
    using difference_type = PtrDiffT;
    using iterator        = TItem*;
    using const_iterator  = const TItem*;


    ////////////////////////////////////////////////////////////
    [[nodiscard]] constexpr InPlaceVector() = default;


    ////////////////////////////////////////////////////////////
    // Trivial for trivially destructible elements, so that e.g. `InPlaceVector<int, N>` is trivially destructible
    constexpr ~InPlaceVector()
        requires za::isTriviallyDestructible<TItem>
    = default;


    ////////////////////////////////////////////////////////////
    constexpr ~InPlaceVector()
    {
        priv::VectorUtils::destroyRange(data(), data() + m_size);
    }


    ////////////////////////////////////////////////////////////
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
    [[nodiscard]] constexpr explicit InPlaceVector(const SizeT initialSize) : m_size{initialSize}
    {
        checkCapacity(initialSize);
        priv::VectorUtils::defaultConstructRange(data(), data() + initialSize);
    }


    ////////////////////////////////////////////////////////////
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
    [[nodiscard]] constexpr explicit InPlaceVector(const SizeT initialSize, const TItem& value) : m_size{initialSize}
    {
        checkCapacity(initialSize);
        priv::VectorUtils::copyConstructRange(data(), data() + initialSize, value);
    }


    ////////////////////////////////////////////////////////////
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
    [[nodiscard]] constexpr explicit InPlaceVector(const TItem* const srcBegin, const TItem* const srcEnd)
    {
        ZA_ASSERT(srcBegin <= srcEnd);
        const auto srcCount = static_cast<SizeT>(srcEnd - srcBegin);
        checkCapacity(srcCount);

        if (srcCount != 0u) // avoid `memcpy(dst, null, 0)` (UB) for an empty range
            priv::VectorUtils::copyRange(data(), srcBegin, srcEnd);

        m_size = srcCount;
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard]] constexpr /* implicit */ InPlaceVector(const std::initializer_list<TItem> iList) :
        InPlaceVector(iList.begin(), iList.end())
    {
    }


    ////////////////////////////////////////////////////////////
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
    [[nodiscard, gnu::always_inline]] constexpr InPlaceVector(const InPlaceVector& rhs) : m_size{rhs.m_size}
    {
        priv::VectorUtils::copyRange(data(), rhs.data(), rhs.data() + m_size);
    }


    ////////////////////////////////////////////////////////////
    // `rhs` may be (part of) an element of `*this`
    constexpr InPlaceVector& operator=(const InPlaceVector& rhs)
    {
        if (this == &rhs)
            return *this;

        // `rhs` lives inside one of our elements: copy it before destroying it
        if (!priv::VectorUtils::isOutsideStorage(data(), data() + m_size, &rhs)) [[unlikely]]
            return *this = InPlaceVector(rhs);

        clear();
        priv::VectorUtils::copyRange(data(), rhs.data(), rhs.data() + rhs.m_size);

        m_size = rhs.m_size;
        return *this;
    }


    ////////////////////////////////////////////////////////////
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
    [[nodiscard, gnu::always_inline]] constexpr InPlaceVector(InPlaceVector&& rhs) noexcept : m_size{rhs.m_size}
    {
        priv::VectorUtils::relocateRange(data(), rhs.data(), rhs.data() + rhs.m_size);
        rhs.m_size = 0u;
    }


    ////////////////////////////////////////////////////////////
    // `rhs` may be (part of) an element of `*this`
    constexpr InPlaceVector& operator=(InPlaceVector&& rhs) noexcept
    {
        if (this == &rhs)
            return *this;

        // `rhs` lives inside one of our elements: take its contents before destroying it
        if (!priv::VectorUtils::isOutsideStorage(data(), data() + m_size, &rhs)) [[unlikely]]
            return *this = InPlaceVector(static_cast<InPlaceVector&&>(rhs));

        clear();

        priv::VectorUtils::relocateRange(data(), rhs.data(), rhs.data() + rhs.m_size);

        m_size     = rhs.m_size;
        rhs.m_size = 0u;

        return *this;
    }


    ////////////////////////////////////////////////////////////
    [[gnu::always_inline]] constexpr void resize(const SizeT newSize, auto&&... args)
    {
        checkCapacity(newSize);

        const auto   oldSize        = m_size;
        TItem* const currentDataPtr = data();

        if (newSize > oldSize)
        {
            for (auto* p = currentDataPtr + oldSize; p != currentDataPtr + newSize; ++p)
                ZA_PLACEMENT_NEW(p) TItem(args...); // intentionally not forwarding
        }
        else if (newSize < oldSize) // Shrinking
        {
            priv::VectorUtils::destroyRange(currentDataPtr + newSize, currentDataPtr + oldSize);
        }

        m_size = newSize;
    }


    ////////////////////////////////////////////////////////////
    template <typename... Ts>
    [[gnu::always_inline]] constexpr TItem* emplace(const TItem* const pos, Ts&&... xs)
    {
        ZA_ASSERT(pos >= begin() && pos <= end());
        checkCapacity(m_size + 1u);

        const auto index = static_cast<SizeT>(pos - data());

        if (index == m_size) // Append at end: no shift, no aliasing risk.
            return &unsafeEmplaceBack(static_cast<Ts&&>(xs)...);

        // Construct a copy first to handle self-aliasing (shifting the elements
        // invalidates any reference into the shifted region).
        TItem copy(static_cast<Ts&&>(xs)...);
        return insertByShifting(index, static_cast<TItem&&>(copy));
    }


    ////////////////////////////////////////////////////////////
    [[gnu::always_inline]] constexpr TItem* insert(const TItem* const pos, const TItem& value)
    {
        return emplace(pos, value);
    }


    ////////////////////////////////////////////////////////////
    [[gnu::always_inline]] constexpr TItem* insert(const TItem* const pos, TItem&& value)
    {
        return emplace(pos, static_cast<TItem&&>(value));
    }


    ////////////////////////////////////////////////////////////
    template <typename T = TItem>
    [[gnu::always_inline]] constexpr TItem& pushBack(T&& x)
    {
        checkCapacity(m_size + 1u);
        return unsafeEmplaceBack(static_cast<T&&>(x));
    }


    ////////////////////////////////////////////////////////////
    template <typename... Ts>
    [[gnu::always_inline]] constexpr TItem& emplaceBack(Ts&&... xs)
    {
        checkCapacity(m_size + 1u);
        return unsafeEmplaceBack(static_cast<Ts&&>(xs)...);
    }


    ////////////////////////////////////////////////////////////
    constexpr void shrinkToFit() noexcept
    {
        // no-op, just for compatibility
    }


    ////////////////////////////////////////////////////////////
    [[gnu::always_inline]] constexpr void reserve(const SizeT targetCapacity)
    {
        checkCapacity(targetCapacity); // otherwise a no-op, just for compatibility
    }


    ////////////////////////////////////////////////////////////
    [[gnu::always_inline]] constexpr void reserveMore(const SizeT n)
    {
        if (n > N - m_size) [[unlikely]] // cannot wrap around, unlike `m_size + n > N`
            priv::inPlaceVectorCapacityExceeded();

        // otherwise a no-op, just for compatibility
    }

    ////////////////////////////////////////////////////////////
    [[gnu::always_inline, gnu::flatten]] constexpr void unsafeEmplaceBackRange(const TItem* const ptr, const SizeT count)
    {
        ZA_ASSERT(m_size + count <= N);
        ZA_ASSERT(count == 0u || ptr != nullptr);

        if (count != 0u) // avoid `memcpy(dst, null, 0)` (UB) when appending nothing
            priv::VectorUtils::copyRange(data() + m_size, ptr, ptr + count);

        m_size += count;
    }

    ////////////////////////////////////////////////////////////
    [[gnu::always_inline, gnu::flatten]] constexpr void clear() noexcept
    {
        priv::VectorUtils::destroyRange(data(), data() + m_size);
        m_size = 0u;
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] constexpr SizeT size() const noexcept
    {
        return m_size;
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::const]] static constexpr SizeT capacity() noexcept
    {
        return N;
    }


    ////////////////////////////////////////////////////////////
    template <typename... Ts>
    [[gnu::always_inline]] constexpr TItem& unsafeEmplaceBack(Ts&&... xs)
    {
        ZA_ASSERT(m_size < N);

        // Size is only increased after a successful construction (no destruction of an unconstructed slot on throw)
        TItem& result = *(ZA_PLACEMENT_NEW(data() + m_size) TItem(static_cast<Ts&&>(xs)...));
        ++m_size;

        return result;
    }


    ////////////////////////////////////////////////////////////
    constexpr TItem* erase(const TItem* const it)
    {
        ZA_ASSERT(it >= begin() && it < end());

        TItem* const d   = data();
        TItem* const pos = d + (it - d);

        m_size = static_cast<SizeT>(priv::VectorUtils::eraseImpl(d + m_size, pos) - d);
        return pos;
    }


    ////////////////////////////////////////////////////////////
    constexpr TItem* erase(const TItem* const first, const TItem* const last)
    {
        ZA_ASSERT(first >= begin() && first <= last && last <= end());

        TItem* const d   = data();
        TItem* const pos = d + (first - d);

        if (first == last)
            return pos; // No elements to erase

        m_size = static_cast<SizeT>(priv::VectorUtils::eraseRangeImpl(d + m_size, pos, d + (last - d)) - d);

        // Elements were shifted into `pos`, or it is the new `end()`.
        return pos;
    }


    ////////////////////////////////////////////////////////////
    template <typename... TItems>
    [[gnu::always_inline]] constexpr void unsafePushBackMultiple(TItems&&... items)
    {
        ZA_ASSERT(m_size + sizeof...(items) <= N);
        (..., (ZA_PLACEMENT_NEW(data() + m_size) TItem(static_cast<TItems&&>(items)), ++m_size));
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::pure]] constexpr TItem* data() noexcept ZA_LIFETIMEBOUND
    {
        return reinterpret_cast<TItem*>(m_storage);
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::pure]] constexpr const TItem* data() const noexcept ZA_LIFETIMEBOUND
    {
        return reinterpret_cast<const TItem*>(m_storage);
    }


    ////////////////////////////////////////////////////////////
    [[gnu::always_inline, gnu::flatten]] constexpr void unsafeSetSize(SizeT newSize) noexcept
    {
        ZA_ASSERT(newSize <= N);
        m_size = newSize;
    }


    ////////////////////////////////////////////////////////////
    [[gnu::always_inline]] constexpr void popBack() noexcept
    {
        ZA_ASSERT(!empty());
        --m_size;

        if constexpr (!ZA_IS_TRIVIALLY_DESTRUCTIBLE(TItem))
            (data() + m_size)->~TItem();
    }


    ////////////////////////////////////////////////////////////
    [[gnu::always_inline]] constexpr void swap(InPlaceVector& rhs) noexcept
    {
        if (this == &rhs)
            return;

        priv::VectorUtils::swapUnequalRanges(data(), m_size, rhs.data(), rhs.m_size);
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::pure]] constexpr TItem& front() noexcept ZA_LIFETIMEBOUND
    {
        ZA_ASSERT(!empty());
        return *data();
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::pure]] constexpr const TItem& front() const noexcept ZA_LIFETIMEBOUND
    {
        ZA_ASSERT(!empty());
        return *data();
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::pure]] constexpr TItem& back() noexcept ZA_LIFETIMEBOUND
    {
        ZA_ASSERT(!empty());
        return *(data() + m_size - 1u);
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::pure]] constexpr const TItem& back() const noexcept ZA_LIFETIMEBOUND
    {
        ZA_ASSERT(!empty());
        return *(data() + m_size - 1u);
    }


    ////////////////////////////////////////////////////////////

private:
    ////////////////////////////////////////////////////////////
    [[gnu::always_inline]] static constexpr void checkCapacity(const SizeT requiredCapacity) noexcept
    {
        if (requiredCapacity > N) [[unlikely]]
            priv::inPlaceVectorCapacityExceeded();
    }


    ////////////////////////////////////////////////////////////
    [[gnu::always_inline]] constexpr void reserveExact(const SizeT targetCapacity) noexcept
    {
        checkCapacity(targetCapacity);
    }


    ////////////////////////////////////////////////////////////
    // Only called by `pushBackMultiple` when the items do not fit
    template <typename... TItems>
    [[noreturn, gnu::always_inline]] constexpr void growAndPushBackMultiple(TItems&&...) noexcept
    {
        priv::inPlaceVectorCapacityExceeded();
    }


    ////////////////////////////////////////////////////////////
    // Only called by `emplaceBackRange` when the range does not fit
    [[noreturn, gnu::always_inline]] constexpr void growAndEmplaceBackRange(const TItem*, SizeT) noexcept
    {
        priv::inPlaceVectorCapacityExceeded();
    }


public:
    ////////////////////////////////////////////////////////////
    ZA_PRIV_DEFINE_COMMON_VECTOR_OPERATIONS(InPlaceVector);
};

} // namespace za
