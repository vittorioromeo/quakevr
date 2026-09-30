#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include "Zancle/Math/MinMaxMacros.hpp"

#include "Zancle/Base/Assert.hpp"
#include "Zancle/Base/AssertAndAssume.hpp"
#include "Zancle/Base/FwdStdAlignedNewDelete.hpp"
#include "Zancle/Base/LifetimeAttributes.hpp"
#include "Zancle/Base/Memcpy.hpp"
#include "Zancle/Base/Memmove.hpp"
#include "Zancle/Base/PlacementNew.hpp"
#include "Zancle/Base/SizeT.hpp"

#include "Zancle/Trait/IsTriviallyCopyable.hpp"
#include "Zancle/Trait/IsTriviallyDestructible.hpp"
#include "Zancle/Trait/IsTriviallyRelocatable.hpp"


namespace za::priv::VectorUtils
{
////////////////////////////////////////////////////////////
template <typename T>
[[gnu::always_inline]] inline constexpr void copyRange(T* target, const T* const srcBegin, const T* const srcEnd)
{
    if constexpr (ZA_IS_TRIVIALLY_COPYABLE(T))
    {
        ZA_MEMCPY(target, srcBegin, sizeof(T) * static_cast<SizeT>(srcEnd - srcBegin));
    }
    else
    {
        for (const T* p = srcBegin; p != srcEnd; ++p, ++target)
            ZA_PLACEMENT_NEW(target) T(*p);
    }
}


////////////////////////////////////////////////////////////
template <typename T>
[[gnu::always_inline]] inline constexpr void destroyRange(T* const srcBegin, T* const srcEnd)
{
    if constexpr (!ZA_IS_TRIVIALLY_DESTRUCTIBLE(T))
    {
        for (T* p = srcBegin; p != srcEnd; ++p)
            p->~T();
    }
}


////////////////////////////////////////////////////////////
/// \brief Move `[srcBegin, srcEnd)` to `target` and end the lifetime of the source objects
///
/// Uses a single `memcpy` for trivially relocatable types, otherwise
/// move-constructs each element and destroys its source (moves are
/// expected not to throw).
///
////////////////////////////////////////////////////////////
template <typename T>
[[gnu::always_inline, gnu::flatten]] inline constexpr void relocateRange(T* target, T* const srcBegin, T* const srcEnd)
{
    if constexpr (ZA_IS_TRIVIALLY_RELOCATABLE(T))
    {
        ZA_MEMCPY(static_cast<void*>(target), srcBegin, sizeof(T) * static_cast<SizeT>(srcEnd - srcBegin));
    }
    else
    {
        for (T* p = srcBegin; p != srcEnd; ++p, ++target)
        {
            ZA_PLACEMENT_NEW(target) T(static_cast<T&&>(*p));
            p->~T();
        }
    }
}


////////////////////////////////////////////////////////////
template <typename T>
[[gnu::always_inline]] inline constexpr void defaultConstructRange(T* const begin, T* const end)
{
    for (T* p = begin; p != end; ++p)
        ZA_PLACEMENT_NEW(p) T();
}


////////////////////////////////////////////////////////////
template <typename T>
[[gnu::always_inline]] inline constexpr void copyConstructRange(T* const begin, T* const end, const T& value)
{
    for (T* p = begin; p != end; ++p)
        ZA_PLACEMENT_NEW(p) T(value);
}


////////////////////////////////////////////////////////////
/// \brief Capacity to grow to when adding elements requires at least `minCapacity` slots
///
/// Geometric (x1.5) growth, floored at 4 so that growing from a tiny
/// capacity (e.g. after `reserve(1)`) does not reallocate on every push
/// (1, 2, 3, 4...).
///
////////////////////////////////////////////////////////////
[[nodiscard, gnu::always_inline, gnu::const]] inline constexpr SizeT grownCapacity(const SizeT currentCapacity,
                                                                                   const SizeT minCapacity) noexcept
{
    const SizeT geometric = currentCapacity + (currentCapacity / 2u);
    return ZA_MAX(minCapacity, ZA_MAX(SizeT{4u}, geometric));
}


////////////////////////////////////////////////////////////
/// \brief Allocate uninitialized storage for `capacity` objects of type `T` (`nullptr` if `capacity == 0`)
///
/// Uses the plain `operator new` unless `T` is over-aligned, so that the
/// common case avoids the aligned allocation path (and allocation hooks
/// that only replace the plain `operator new` see the allocation).
///
////////////////////////////////////////////////////////////
template <typename T>
[[nodiscard, gnu::always_inline]] inline T* allocate(const SizeT capacity)
{
    ZA_ASSERT(capacity <= SizeT(-1) / sizeof(T) && "allocation size overflow");

    if (capacity == 0u)
        return nullptr;

    if constexpr (alignof(T) > __STDCPP_DEFAULT_NEW_ALIGNMENT__)
        return static_cast<T*>(::operator new(capacity * sizeof(T), std::align_val_t{alignof(T)}));
    else
        return static_cast<T*>(::operator new(capacity * sizeof(T)));
}


////////////////////////////////////////////////////////////
/// \brief Free storage obtained from `allocate<T>(capacity)` (sized deallocation)
///
////////////////////////////////////////////////////////////
template <typename T>
[[gnu::always_inline]] inline void deallocate(T* const p, const SizeT capacity) noexcept
{
    if constexpr (alignof(T) > __STDCPP_DEFAULT_NEW_ALIGNMENT__)
        ::operator delete(p, capacity * sizeof(T), std::align_val_t{alignof(T)});
    else
        ::operator delete(p, capacity * sizeof(T));
}


////////////////////////////////////////////////////////////
/// \brief Erase the element at `it`, shifting `[it+1, end)` left by 1
///
/// Takes full responsibility for destruction: the slot at `end - 1`
/// is left uninitialized after the call. The caller must update its
/// size to the returned pointer and must NOT destroy the trailing slot.
///
/// \return Pointer one past the last live element (i.e. `end - 1`)
///
////////////////////////////////////////////////////////////
template <typename T>
[[gnu::always_inline, gnu::flatten]] inline constexpr T* eraseImpl(T* const end, T* const it)
{
    ZA_ASSERT(it < end);

    T* const nextElement = it + 1;

    if constexpr (ZA_IS_TRIVIALLY_RELOCATABLE(T))
    {
        // For non-trivially-destructible types we must release the resources
        // held by `*it` before its bytes get overwritten by the memmove
        // (otherwise we leak). For trivially destructible types (which include
        // all trivially copyable types) the destroy is a no-op.
        if constexpr (!ZA_IS_TRIVIALLY_DESTRUCTIBLE(T))
            it->~T();

        // Bulk shift `[it+1, end)` over `[it, end-1)`.
        if (nextElement < end)
            ZA_MEMMOVE(static_cast<void*>(it), nextElement, static_cast<SizeT>(end - nextElement) * sizeof(T));

        // The slot at `end - 1` now holds duplicate / dead bytes -- caller must not dtor it.
        return end - 1;
    }
    else
    {
        // Generic path: shift via move-assignment, then destroy whatever
        // remains in the trailing slot (either the original element if
        // erasing the last one, or a moved-from element after the shift).
        T* currWrite = it;
        T* currRead  = nextElement;

        while (currRead != end)
            *currWrite++ = static_cast<T&&>(*currRead++);

        // `currWrite == end - 1`; destroy the trailing slot.
        currWrite->~T();
        return currWrite;
    }
}


////////////////////////////////////////////////////////////
/// \brief Erase the half-open range `[first, last)`, shifting the tail left
///
/// Same destruction contract as `eraseImpl`: returns one past the last
/// live element; trailing slots are left uninitialized.
///
////////////////////////////////////////////////////////////
template <typename T>
[[gnu::always_inline, gnu::flatten]] inline constexpr T* eraseRangeImpl(T* const end, T* const first, T* const last)
{
    ZA_ASSERT(first <= last);
    ZA_ASSERT(last <= end);
    ZA_ASSERT(first != last);

    if constexpr (ZA_IS_TRIVIALLY_RELOCATABLE(T))
    {
        // Release resources held by the erased elements before their bytes
        // are overwritten / abandoned. No-op for trivially destructible T.
        destroyRange(first, last);

        // Bulk shift `[last, end)` over `[first, first + (end - last))`.
        if (last < end)
            ZA_MEMMOVE(static_cast<void*>(first), last, static_cast<SizeT>(end - last) * sizeof(T));

        // Slots past `first + (end - last)` are now dead bytes -- caller must not dtor them.
        return first + (end - last);
    }
    else
    {
        // Generic path: shift via move-assignment, then destroy the tail.
        T* currWrite = first;

        T* currRead = last;
        while (currRead != end)
            *currWrite++ = static_cast<T&&>(*currRead++);

        // The tail `[currWrite, end)` covers both still-original elements
        // (when more were erased than shifted) and moved-from elements
        // (after the shift). Both need their destructors called.
        destroyRange(currWrite, end);
        return currWrite;
    }
}


////////////////////////////////////////////////////////////
template <typename T>
[[gnu::always_inline, gnu::flatten]] inline constexpr void makeHole(T* const pos, T* const end)
{
    // Moves elements in `[pos, end)` to `[pos + 1, end + 1)`.
    // Assumes capacity is sufficient.
    ZA_ASSERT(pos <= end);

    if (pos == end)
        return; // Inserting at the end, no move needed.

    if constexpr (ZA_IS_TRIVIALLY_RELOCATABLE(T))
    {
        ZA_MEMMOVE(static_cast<void*>(pos + 1),                // Destination
                   pos,                                        // Source
                   static_cast<SizeT>(end - pos) * sizeof(T)); // Number of bytes
    }
    else
    {
        // Move-construct a new element at the end from the old last element.
        ZA_PLACEMENT_NEW(end) T(static_cast<T&&>(*(end - 1)));

        // Move-assign elements backwards to shift them to the right.
        for (T* p = end - 1; p > pos; --p)
            *p = static_cast<T&&>(*(p - 1));

        // The element at `pos` has been moved from, but its memory is still valid.
        // It will be overwritten by the new element. If it's not trivially
        // destructible, we should destroy it first to release its resources.
        if constexpr (!ZA_IS_TRIVIALLY_DESTRUCTIBLE(T))
            pos->~T();
    }
}


////////////////////////////////////////////////////////////
/// \brief `true` iff `p` lies outside the storage range `[begin, end)`
///
/// Used to detect arguments that alias an element of the container
/// being modified (e.g. debug assertions in `resize`, or to pick a
/// safe path in `assignRange` and the copy/move assignments).
///
////////////////////////////////////////////////////////////
template <typename T, typename U>
[[nodiscard, gnu::always_inline]] inline bool isOutsideStorage(const T* const begin, const T* const end, const U* const p) noexcept
{
    const void* const pv = static_cast<const void*>(p);
    return pv < static_cast<const void*>(begin) || pv >= static_cast<const void*>(end);
}


////////////////////////////////////////////////////////////
/// \brief Operations shared by `Vector`, `SmallVector`, and `InPlaceVector`
///
/// Requires the vector type to provide `data()`, `size()`, `capacity()`,
/// `reserve(n)`, `reserveExact(n)`, `clear()`, `unsafeSetSize(n)`,
/// `erase(first, last)`, `unsafePushBackMultiple(xs...)`,
/// `unsafeEmplaceBackRange(ptr, count)`, `growAndPushBackMultiple(xs...)`,
/// and `growAndEmplaceBackRange(ptr, count)`. Must be the last thing in
/// the class, as it ends with a `private:` section.
///
////////////////////////////////////////////////////////////
#define ZA_PRIV_DEFINE_COMMON_VECTOR_OPERATIONS(vectorType)                                                                         \
                                                                                                                                    \
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::pure]] constexpr TItem& operator[](const SizeT i) noexcept ZA_LIFETIMEBOUND \
    {                                                                                                                               \
        ZA_ASSERT(i < size());                                                                                                      \
        return *(data() + i);                                                                                                       \
    }                                                                                                                               \
                                                                                                                                    \
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::pure]] constexpr const TItem& operator[](const SizeT i)                     \
        const noexcept ZA_LIFETIMEBOUND                                                                                             \
    {                                                                                                                               \
        ZA_ASSERT(i < size());                                                                                                      \
        return *(data() + i);                                                                                                       \
    }                                                                                                                               \
                                                                                                                                    \
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::pure]] constexpr TItem* begin() noexcept ZA_LIFETIMEBOUND                   \
    {                                                                                                                               \
        return data();                                                                                                              \
    }                                                                                                                               \
                                                                                                                                    \
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::pure]] constexpr const TItem* begin() const noexcept ZA_LIFETIMEBOUND       \
    {                                                                                                                               \
        return data();                                                                                                              \
    }                                                                                                                               \
                                                                                                                                    \
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::pure]] constexpr TItem* end() noexcept ZA_LIFETIMEBOUND                     \
    {                                                                                                                               \
        return data() + size();                                                                                                     \
    }                                                                                                                               \
                                                                                                                                    \
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::pure]] constexpr const TItem* end() const noexcept ZA_LIFETIMEBOUND         \
    {                                                                                                                               \
        return data() + size();                                                                                                     \
    }                                                                                                                               \
                                                                                                                                    \
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::pure]] constexpr const TItem* cbegin() const noexcept ZA_LIFETIMEBOUND      \
    {                                                                                                                               \
        return data();                                                                                                              \
    }                                                                                                                               \
                                                                                                                                    \
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::pure]] constexpr const TItem* cend() const noexcept ZA_LIFETIMEBOUND        \
    {                                                                                                                               \
        return data() + size();                                                                                                     \
    }                                                                                                                               \
                                                                                                                                    \
    [[nodiscard, gnu::always_inline, gnu::pure]] constexpr bool empty() const noexcept                                              \
    {                                                                                                                               \
        return size() == 0u;                                                                                                        \
    }                                                                                                                               \
                                                                                                                                    \
    /* No `this == &rhs` shortcut: element-wise semantics (e.g. NaN != NaN) apply to self-comparison too */                         \
    [[nodiscard]] constexpr bool operator==(const vectorType& rhs) const                                                            \
    {                                                                                                                               \
        const SizeT lhsSize = size();                                                                                               \
                                                                                                                                    \
        if (lhsSize != rhs.size())                                                                                                  \
            return false;                                                                                                           \
                                                                                                                                    \
        const TItem* rhsIt = rhs.data();                                                                                            \
                                                                                                                                    \
        for (const TItem *lhsIt = data(), *const lhsEnd = lhsIt + lhsSize; lhsIt != lhsEnd; ++lhsIt, ++rhsIt)                       \
            if (*lhsIt != *rhsIt)                                                                                                   \
                return false;                                                                                                       \
                                                                                                                                    \
        return true;                                                                                                                \
    }                                                                                                                               \
                                                                                                                                    \
    /* NOLINTNEXTLINE(bugprone-macro-parentheses) */                                                                                \
    [[gnu::always_inline]] constexpr friend void swap(vectorType& lhs, vectorType& rhs) noexcept                                    \
    {                                                                                                                               \
        lhs.swap(rhs);                                                                                                              \
    }                                                                                                                               \
                                                                                                                                    \
    /* \pre `xs...` must not reference `*it`, and the constructor must not throw (the old element is destroyed first) */                                                                                                                                 \
    template <typename... Ts>                                                                                                       \
    [[gnu::always_inline]] constexpr TItem& reEmplaceByIterator(TItem* const it, Ts&&... xs)                                        \
    {                                                                                                                               \
        ZA_ASSERT(it >= begin() && it < end());                                                                                     \
        ZA_ASSERT((priv::VectorUtils::isOutsideStorage(it, it + 1, &xs) && ...));                                                   \
                                                                                                                                    \
        if constexpr (!ZA_IS_TRIVIALLY_DESTRUCTIBLE(TItem))                                                                         \
            it->~TItem();                                                                                                           \
                                                                                                                                    \
        return *(ZA_PLACEMENT_NEW(it) TItem(static_cast<Ts&&>(xs)...));                                                             \
    }                                                                                                                               \
                                                                                                                                    \
    /* \pre See `reEmplaceByIterator` */                                                                                            \
    template <typename... Ts>                                                                                                       \
    [[gnu::always_inline]] constexpr TItem& reEmplaceByIndex(const SizeT index, Ts&&... xs)                                         \
    {                                                                                                                               \
        return reEmplaceByIterator(data() + index, static_cast<Ts&&>(xs)...);                                                       \
    }                                                                                                                               \
                                                                                                                                    \
    /* `items...` may reference elements of `*this` (handled on growth) */                                                          \
    template <typename... TItems>                                                                                                   \
    [[gnu::always_inline]] constexpr void pushBackMultiple(TItems&&... items)                                                       \
    {                                                                                                                               \
        if constexpr (sizeof...(items) > 0u)                                                                                        \
        {                                                                                                                           \
            if (size() + sizeof...(items) > capacity()) [[unlikely]]                                                                \
                return growAndPushBackMultiple(static_cast<TItems&&>(items)...);                                                    \
                                                                                                                                    \
            unsafePushBackMultiple(static_cast<TItems&&>(items)...);                                                                \
        }                                                                                                                           \
    }                                                                                                                               \
                                                                                                                                    \
    /* Append copies of the `count` elements starting at `ptr`, which may point into `*this` (handled on growth) */                 \
    [[gnu::always_inline]] constexpr void emplaceBackRange(const TItem* const ptr, const SizeT count)                               \
    {                                                                                                                               \
        if (size() + count > capacity()) [[unlikely]]                                                                               \
            return growAndEmplaceBackRange(ptr, count);                                                                             \
                                                                                                                                    \
        unsafeEmplaceBackRange(ptr, count);                                                                                         \
    }                                                                                                                               \
                                                                                                                                    \
    [[gnu::always_inline, gnu::flatten]] constexpr void unsafeEmplaceOther(const vectorType& rhs)                                   \
    {                                                                                                                               \
        unsafeEmplaceBackRange(rhs.data(), rhs.size());                                                                             \
    }                                                                                                                               \
                                                                                                                                    \
    /* Replace the contents with copies of `[b, e)`, which may be a subrange of `*this` */                                          \
    constexpr void assignRange(const TItem* const b, const TItem* const e)                                                          \
    {                                                                                                                               \
        ZA_ASSERT(b <= e);                                                                                                          \
        ZA_ASSERT(b == e || (b != nullptr && e != nullptr)); /* only a non-empty range must be non-null */                          \
                                                                                                                                    \
        if (b != e && !priv::VectorUtils::isOutsideStorage(data(), data() + size(), b)) [[unlikely]]                                \
        {                                                                                                                           \
            /* `[b, e)` is a non-empty subrange of `*this`: keep it by erasing what surrounds it */                                 \
            TItem* const first = data() + (b - data());                                                                             \
            erase(first + (e - b), end());                                                                                          \
            erase(data(), first);                                                                                                   \
            return;                                                                                                                 \
        }                                                                                                                           \
                                                                                                                                    \
        const auto count = static_cast<SizeT>(e - b);                                                                               \
                                                                                                                                    \
        clear();                                                                                                                    \
        reserveExact(count);                                                                                                        \
                                                                                                                                    \
        if (count != 0u) /* avoid `memcpy(null, null, 0)` (UB) on the empty-range path */                                           \
            priv::VectorUtils::copyRange(data(), b, e);                                                                             \
                                                                                                                                    \
        unsafeSetSize(count);                                                                                                       \
    }                                                                                                                               \
                                                                                                                                    \
    [[gnu::always_inline]] constexpr void eraseAt(const SizeT index)                                                                \
    {                                                                                                                               \
        ZA_ASSERT(index < size());                                                                                                  \
        erase(data() + index);                                                                                                      \
    }                                                                                                                               \
                                                                                                                                    \
private:                                                                                                                            \
    /* Middle insertion (kept out of line, so that `emplace` call sites only inline the append fast path) */                        \
    [[gnu::noinline]] constexpr TItem* insertByShifting(const SizeT index, TItem&& value)                                           \
    {                                                                                                                               \
        ZA_ASSERT(size() < capacity());                                                                                             \
                                                                                                                                    \
        TItem* const pos = data() + index;                                                                                          \
        priv::VectorUtils::makeHole(pos, end());                                                                                    \
        ZA_PLACEMENT_NEW(pos) TItem(static_cast<TItem&&>(value));                                                                   \
                                                                                                                                    \
        unsafeSetSize(size() + 1u);                                                                                                 \
        return pos;                                                                                                                 \
    }                                                                                                                               \
                                                                                                                                    \
    static_assert(true)


////////////////////////////////////////////////////////////
/// \brief Growth operations shared by `Vector` and `SmallVector`
///
/// Requires the vector type to provide
/// `adoptGrownStorage(newData, newCapacity, gapBegin, gapCount)`, which
/// relocates the current elements into `newData` around a gap of
/// `gapCount` already-constructed elements at `gapBegin`, frees the old
/// storage, and adopts the new one. New elements are always constructed
/// before the old storage is freed, so arguments referencing elements
/// of the vector itself remain valid.
///
////////////////////////////////////////////////////////////
#define ZA_PRIV_DEFINE_GROWABLE_VECTOR_OPERATIONS                                                                 \
                                                                                                                  \
    /* Explicit reservations are not floored (e.g. `reserve(1)` on an empty vector allocates exactly 1 slot) */   \
    [[gnu::cold, gnu::noinline]] void reserveImpl(const SizeT minCapacity)                                        \
    {                                                                                                             \
        const SizeT currentCapacity = capacity();                                                                 \
        const SizeT newCapacity     = ZA_MAX(minCapacity, currentCapacity + (currentCapacity / 2u));              \
        adoptGrownStorage(priv::VectorUtils::allocate<TItem>(newCapacity), newCapacity, size(), 0u);              \
    }                                                                                                             \
                                                                                                                  \
    [[gnu::cold, gnu::noinline]] void reserveExactImpl(const SizeT newCapacity)                                   \
    {                                                                                                             \
        adoptGrownStorage(priv::VectorUtils::allocate<TItem>(newCapacity), newCapacity, size(), 0u);              \
    }                                                                                                             \
                                                                                                                  \
    /* Allocate exactly `targetCapacity` slots if the current capacity is smaller (no geometric growth) */        \
    [[gnu::always_inline]] void reserveExact(const SizeT targetCapacity)                                          \
    {                                                                                                             \
        if (capacity() < targetCapacity)                                                                          \
            reserveExactImpl(targetCapacity);                                                                     \
    }                                                                                                             \
                                                                                                                  \
    template <typename... Ts>                                                                                     \
    [[gnu::cold, gnu::noinline, gnu::returns_nonnull]] TItem* growAndEmplace(const SizeT insertIndex, Ts&&... xs) \
    {                                                                                                             \
        const SizeT  newCapacity = priv::VectorUtils::grownCapacity(capacity(), size() + 1u);                     \
        TItem* const newData     = priv::VectorUtils::allocate<TItem>(newCapacity);                               \
        ZA_ASSERT_AND_ASSUME(newData != nullptr);                                                                 \
                                                                                                                  \
        ZA_PLACEMENT_NEW(newData + insertIndex) TItem(static_cast<Ts&&>(xs)...);                                  \
        adoptGrownStorage(newData, newCapacity, insertIndex, 1u);                                                 \
                                                                                                                  \
        return newData + insertIndex;                                                                             \
    }                                                                                                             \
                                                                                                                  \
    template <typename... TItems>                                                                                 \
    [[gnu::cold, gnu::noinline]] void growAndPushBackMultiple(TItems&&... items)                                  \
    {                                                                                                             \
        const SizeT  oldSize     = size();                                                                        \
        const SizeT  newCapacity = priv::VectorUtils::grownCapacity(capacity(), oldSize + sizeof...(items));      \
        TItem* const newData     = priv::VectorUtils::allocate<TItem>(newCapacity);                               \
        ZA_ASSERT_AND_ASSUME(newData != nullptr);                                                                 \
                                                                                                                  \
        TItem* slot = newData + oldSize;                                                                          \
        (..., ZA_PLACEMENT_NEW(slot++) TItem(static_cast<TItems&&>(items)));                                      \
        adoptGrownStorage(newData, newCapacity, oldSize, sizeof...(items));                                       \
    }                                                                                                             \
                                                                                                                  \
    [[gnu::cold, gnu::noinline]] void growAndEmplaceBackRange(const TItem* const ptr, const SizeT count)          \
    {                                                                                                             \
        ZA_ASSERT(count > 0u && ptr != nullptr); /* only called when growing, i.e. when appending something */    \
                                                                                                                  \
        const SizeT  oldSize     = size();                                                                        \
        const SizeT  newCapacity = priv::VectorUtils::grownCapacity(capacity(), oldSize + count);                 \
        TItem* const newData     = priv::VectorUtils::allocate<TItem>(newCapacity);                               \
                                                                                                                  \
        priv::VectorUtils::copyRange(newData + oldSize, ptr, ptr + count);                                        \
        adoptGrownStorage(newData, newCapacity, oldSize, count);                                                  \
    }                                                                                                             \
                                                                                                                  \
    static_assert(true)

} // namespace za::priv::VectorUtils


////////////////////////////////////////////////////////////
/// \file
///
/// \brief Shared low-level helpers for `Vector`, `SmallVector`, `InPlaceVector`
///
/// All non-trivial element-management primitives (allocate / deallocate,
/// construct / destroy / relocate ranges, erase, the shared-operation
/// macros) live here so that the three vector flavors can share their
/// implementation without inheritance or virtual dispatch. The helpers
/// branch on trivial relocatability and trivial destructibility to fall
/// back to `memcpy`/`memmove` whenever the element type permits it.
///
/// Exceptions are not officially supported: the containers only avoid
/// double destruction when an element constructor throws, and moves are
/// expected not to throw.
///
////////////////////////////////////////////////////////////
