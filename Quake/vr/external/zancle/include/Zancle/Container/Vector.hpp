#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include "Zancle/Container/Priv/VectorUtils.hpp"

#include "Zancle/Base/Assert.hpp"
#include "Zancle/Base/InitializerList.hpp"
#include "Zancle/Base/LifetimeAttributes.hpp"
#include "Zancle/Base/PlacementNew.hpp"
#include "Zancle/Base/PtrDiffT.hpp"
#include "Zancle/Base/SizeT.hpp"

#include "Zancle/Trait/EnableTrivialRelocation.hpp"
#include "Zancle/Trait/IsTriviallyDestructible.hpp"


namespace za
{
////////////////////////////////////////////////////////////
/// \brief Heap-allocated dynamic array, lightweight `std::vector` replacement
///
/// Provides the usual `std::vector` interface plus a few opinionated
/// extras (`unsafeEmplaceBack`, `unsafePushBackMultiple`,
/// `reserveMore`, `unsafeSetSize`) tuned for hot paths where the caller
/// has already ensured enough capacity.
///
/// Implementation notes:
/// - Storage is tracked with three pointers (`begin`, `end`, `endCap`)
///   to make `size()`, `capacity()`, and `data()` extremely cheap and
///   to avoid recomputing offsets in tight loops.
/// - Growth is geometric (x1.5) and never less than what is required;
///   growth caused by adding elements is floored at 4 elements.
///   Constructors and assignments allocate exactly.
/// - Growing operations construct the new elements before releasing the
///   old storage, so their arguments may reference existing elements
///   (e.g. `v.pushBack(v[0])`, `v.emplaceBackRange(v.data(), v.size())`).
/// - `Vector` is itself trivially relocatable.
/// - Trivially relocatable element types are moved with `memcpy` rather
///   than per-element move constructors.
/// - Avoids the heavy `<vector>` standard header.
///
////////////////////////////////////////////////////////////
template <typename TItem>
class [[nodiscard]] ZA_GSL_OWNER(TItem) Vector
{
private:
    ////////////////////////////////////////////////////////////
    // Member data
    ////////////////////////////////////////////////////////////
    TItem* m_data{nullptr};        //!< Pointer to the beginning of the storage, or `nullptr`
    TItem* m_endSize{nullptr};     //!< Pointer one past the last constructed element
    TItem* m_endCapacity{nullptr}; //!< Pointer one past the end of the allocated storage


    ////////////////////////////////////////////////////////////
    /// \brief Relocate the elements into `newData` around a gap, free the old storage, and adopt the new one
    ///
    /// `newData` has room for `newCapacity` elements, and its slots
    /// `[gapBegin, gapBegin + gapCount)` hold already-constructed elements.
    ///
    ////////////////////////////////////////////////////////////
    [[gnu::cold, gnu::noinline]] void adoptGrownStorage(TItem* const newData,
                                                        const SizeT  newCapacity,
                                                        const SizeT  gapBegin,
                                                        const SizeT  gapCount)
    {
        const SizeT oldSize = size();

        ZA_ASSERT(gapBegin <= oldSize);
        ZA_ASSERT(oldSize + gapCount <= newCapacity);

        if (m_data != nullptr)
        {
            priv::VectorUtils::relocateRange(newData, m_data, m_data + gapBegin);
            priv::VectorUtils::relocateRange(newData + gapBegin + gapCount, m_data + gapBegin, m_endSize);
            priv::VectorUtils::deallocate(m_data, capacity());
        }

        m_data        = newData;
        m_endSize     = newData + oldSize + gapCount;
        m_endCapacity = newData + newCapacity;
    }


    ////////////////////////////////////////////////////////////
    ZA_PRIV_DEFINE_GROWABLE_VECTOR_OPERATIONS;


public:
    ////////////////////////////////////////////////////////////
    ZA_ENABLE_TRIVIAL_RELOCATION;


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
    /// \brief Default constructor, creates an empty vector with no allocation
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] Vector() = default;


    ////////////////////////////////////////////////////////////
    /// \brief Destructor, destroys all elements and frees the storage
    ///
    ////////////////////////////////////////////////////////////
    ~Vector()
    {
        priv::VectorUtils::destroyRange(m_data, m_endSize);
        priv::VectorUtils::deallocate(m_data, capacity());
    }


    ////////////////////////////////////////////////////////////
    /// \brief Construct with `initialSize` default-constructed elements
    ///
    ////////////////////////////////////////////////////////////
    // Delegating to the default constructor makes the destructor free the storage if a constructor throws
    [[nodiscard]] explicit Vector(const SizeT initialSize) : Vector()
    {
        if (initialSize == 0u)
            return;

        m_endSize = m_data = priv::VectorUtils::allocate<TItem>(initialSize);
        m_endCapacity      = m_data + initialSize;

        priv::VectorUtils::defaultConstructRange(m_data, m_endCapacity);
        m_endSize = m_endCapacity;
    }


    ////////////////////////////////////////////////////////////
    /// \brief Construct with `initialSize` copies of `value`
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] explicit Vector(const SizeT initialSize, const TItem& value) : Vector()
    {
        if (initialSize == 0u)
            return;

        m_endSize = m_data = priv::VectorUtils::allocate<TItem>(initialSize);
        m_endCapacity      = m_data + initialSize;

        priv::VectorUtils::copyConstructRange(m_data, m_endCapacity, value);
        m_endSize = m_endCapacity;
    }


    ////////////////////////////////////////////////////////////
    /// \brief Construct by copying the range `[srcBegin, srcEnd)`
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] explicit Vector(const TItem* const srcBegin, const TItem* const srcEnd) : Vector()
    {
        ZA_ASSERT(srcBegin <= srcEnd);
        const auto srcCount = static_cast<SizeT>(srcEnd - srcBegin);

        if (srcCount == 0u)
            return;

        m_endSize = m_data = priv::VectorUtils::allocate<TItem>(srcCount);
        m_endCapacity      = m_data + srcCount;

        priv::VectorUtils::copyRange(m_data, srcBegin, srcEnd);
        m_endSize = m_endCapacity;
    }


    ////////////////////////////////////////////////////////////
    /// \brief Construct from a brace-enclosed initializer list
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] /* implicit */ Vector(const std::initializer_list<TItem> iList) : Vector(iList.begin(), iList.end())
    {
    }


    ////////////////////////////////////////////////////////////
    /// \brief Copy constructor
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline]] Vector(const Vector& rhs) : Vector(rhs.m_data, rhs.m_endSize)
    {
    }


    ////////////////////////////////////////////////////////////
    /// \brief Copy assignment
    ///
    /// `rhs` may be (part of) an element of `*this`.
    ///
    ////////////////////////////////////////////////////////////
    Vector& operator=(const Vector& rhs)
    {
        if (this == &rhs)
            return *this;

        // `rhs` lives inside one of our elements (e.g. `v = v[0].children`): copy it before destroying it
        if (!priv::VectorUtils::isOutsideStorage(m_data, m_endSize, &rhs)) [[unlikely]]
            return *this = Vector(rhs);

        assignRange(rhs.m_data, rhs.m_endSize);
        return *this;
    }


    ////////////////////////////////////////////////////////////
    /// \brief Move constructor (steals storage from `rhs`)
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline]] Vector(Vector&& rhs) noexcept :
        m_data{rhs.m_data},
        m_endSize{rhs.m_endSize},
        m_endCapacity{rhs.m_endCapacity}
    {
        rhs.m_data    = nullptr;
        rhs.m_endSize = rhs.m_endCapacity = nullptr;
    }


    ////////////////////////////////////////////////////////////
    /// \brief Move assignment (steals storage from `rhs`, then frees the existing storage)
    ///
    /// `rhs` may be (part of) an element of `*this`: its storage is taken
    /// over before the existing elements are destroyed.
    ///
    ////////////////////////////////////////////////////////////
    Vector& operator=(Vector&& rhs) noexcept
    {
        if (this == &rhs)
            return *this;

        TItem* const newData        = rhs.m_data;
        TItem* const newEndSize     = rhs.m_endSize;
        TItem* const newEndCapacity = rhs.m_endCapacity;

        rhs.m_data    = nullptr;
        rhs.m_endSize = rhs.m_endCapacity = nullptr;

        priv::VectorUtils::destroyRange(m_data, m_endSize);
        priv::VectorUtils::deallocate(m_data, capacity());

        m_data        = newData;
        m_endSize     = newEndSize;
        m_endCapacity = newEndCapacity;

        return *this;
    }


    ////////////////////////////////////////////////////////////
    /// \brief Resize to `newSize` elements
    ///
    /// When growing, new elements are constructed from `args...` (intentionally
    /// not perfect-forwarded, so the same args can be reused for every new
    /// element). When shrinking, trailing elements are destroyed; capacity is
    /// unchanged.
    ///
    /// \warning When growing, `args...` must not reference an element of
    ///          `*this`: the growth reallocates and frees the current
    ///          buffer before constructing the new elements, so such a
    ///          reference would dangle (e.g. `v.resize(v.size() * 2, v[0])`
    ///          is undefined). Debug-asserted.
    ///
    ////////////////////////////////////////////////////////////
    [[gnu::always_inline]] void resize(const SizeT newSize, auto&&... args)
    {
        const auto oldSize = size();

        if (newSize > oldSize)
        {
            // See \warning above: fill args must not alias existing elements.
            ZA_ASSERT((priv::VectorUtils::isOutsideStorage(m_data, m_endSize, &args) && ...));

            reserve(newSize);

            for (auto* p = m_data + oldSize; p != m_data + newSize; ++p)
                ZA_PLACEMENT_NEW(p) TItem(args...); // intentionally not forwarding
        }
        else
        {
            priv::VectorUtils::destroyRange(m_data + newSize, m_endSize);
        }

        m_endSize = m_data + newSize;
    }


    ////////////////////////////////////////////////////////////
    /// \brief Construct in-place at iterator `pos`; invalidates iterators on growth
    ///
    /// `pos` is recomputed from its index across a potential reallocation,
    /// and `xs...` may reference elements of `*this`.
    ///
    /// \return Iterator to the newly inserted element
    ///
    ////////////////////////////////////////////////////////////
    template <typename... Ts>
    [[gnu::always_inline]] TItem* emplace(const TItem* const pos, Ts&&... xs)
    {
        ZA_ASSERT(pos >= begin() && pos <= end());

        const auto index = static_cast<SizeT>(pos - m_data);

        if (m_endSize == m_endCapacity) [[unlikely]]
            return growAndEmplace(index, static_cast<Ts&&>(xs)...);

        if (pos == m_endSize) // Append at end: no shift, no aliasing risk.
            return &unsafeEmplaceBack(static_cast<Ts&&>(xs)...);

        // Construct a copy first to handle self-aliasing (shifting the elements
        // invalidates any reference into the shifted region).
        TItem copy(static_cast<Ts&&>(xs)...);
        return insertByShifting(index, static_cast<TItem&&>(copy));
    }


    ////////////////////////////////////////////////////////////
    /// \brief Insert a copy of `value` at position `pos`
    ///
    ////////////////////////////////////////////////////////////
    [[gnu::always_inline]] TItem* insert(const TItem* const pos, const TItem& value)
    {
        return emplace(pos, value);
    }


    ////////////////////////////////////////////////////////////
    /// \brief Insert a moved-from `value` at position `pos`
    ///
    ////////////////////////////////////////////////////////////
    [[gnu::always_inline]] TItem* insert(const TItem* const pos, TItem&& value)
    {
        return emplace(pos, static_cast<TItem&&>(value));
    }


    ////////////////////////////////////////////////////////////
    /// \brief Append a copy or moved-from element (which may be an element of `*this`)
    ///
    ////////////////////////////////////////////////////////////
    template <typename T = TItem>
    [[gnu::always_inline]] TItem& pushBack(T&& x)
    {
        if (m_endSize != m_endCapacity) [[likely]]
            return unsafeEmplaceBack(static_cast<T&&>(x));

        return *growAndEmplace(size(), static_cast<T&&>(x));
    }


    ////////////////////////////////////////////////////////////
    /// \brief Construct a new element in-place at the end (`xs...` may reference elements of `*this`)
    ///
    ////////////////////////////////////////////////////////////
    template <typename... Ts>
    [[gnu::always_inline]] TItem& emplaceBack(Ts&&... xs)
    {
        if (m_endSize != m_endCapacity) [[likely]]
            return unsafeEmplaceBack(static_cast<Ts&&>(xs)...);

        return *growAndEmplace(size(), static_cast<Ts&&>(xs)...);
    }


    ////////////////////////////////////////////////////////////
    /// \brief Shrink the allocated storage to match `size()`
    ///
    /// If `size() == 0`, the buffer is freed entirely. Otherwise the
    /// elements are relocated into a freshly allocated buffer of exact
    /// size.
    ///
    ////////////////////////////////////////////////////////////
    void shrinkToFit()
    {
        const SizeT currentSize = size();

        if (capacity() <= currentSize)
            return;

        if (currentSize == 0u)
        {
            priv::VectorUtils::deallocate(m_data, capacity());

            m_data    = nullptr;
            m_endSize = m_endCapacity = nullptr;

            return;
        }

        auto* newData = priv::VectorUtils::allocate<TItem>(currentSize);

        priv::VectorUtils::relocateRange(newData, m_data, m_endSize);
        priv::VectorUtils::deallocate(m_data, capacity());

        m_data    = newData;
        m_endSize = m_endCapacity = m_data + currentSize;
    }


    ////////////////////////////////////////////////////////////
    /// \brief Ensure capacity is at least `targetCapacity`
    ///
    /// Grows geometrically (see the class documentation), so that
    /// repeated reservations for appends are amortized.
    ///
    /// \return Reference to the internal end-of-size pointer to enable hot-path fused reserve+write idioms
    ///
    ////////////////////////////////////////////////////////////
    [[gnu::always_inline]] TItem*& reserve(const SizeT targetCapacity)
    {
        if (capacity() < targetCapacity) [[unlikely]]
            reserveImpl(targetCapacity);

        return m_endSize;
    }


    ////////////////////////////////////////////////////////////
    /// \brief Allocate exactly `targetCapacity` slots without ever
    ///        invoking element move/copy/destroy
    ///
    /// This is the only way to grow a `Vector<T>` when `T` is
    /// non-movable (e.g. `Worker` in the thread pool, where worker
    /// threads capture `this` and the type is intentionally pinned).
    /// Standard `reserve` / `resize` would fail to compile because they
    /// instantiate the relocation path, which requires `T(T&&)`.
    ///
    /// \pre `size() == 0` -- any existing buffer is discarded by
    ///      `operator delete` without running destructors. Calling this
    ///      on a non-empty vector leaks resources held by the existing
    ///      elements (debug-asserted).
    ///
    /// \param targetCapacity Number of element slots to allocate
    ///
    /// \return Reference to the internal end-of-size pointer (matches
    ///         `reserve()` for the hot-path fused reserve+write idiom).
    ////////////////////////////////////////////////////////////
    [[gnu::always_inline]] TItem*& unsafeAllocateCapacity(const SizeT targetCapacity)
    {
        ZA_ASSERT(size() == 0u);

        auto* newData = priv::VectorUtils::allocate<TItem>(targetCapacity);
        priv::VectorUtils::deallocate(m_data, capacity());

        m_data        = newData;
        m_endSize     = m_data;
        m_endCapacity = m_data + targetCapacity;

        return m_endSize;
    }


    ////////////////////////////////////////////////////////////
    /// \brief Ensure capacity is at least `size() + n`
    ///
    /// Convenience wrapper around `reserve()`, useful when bulk-appending
    /// `n` known elements.
    ///
    ////////////////////////////////////////////////////////////
    [[gnu::always_inline, gnu::flatten]] TItem*& reserveMore(const SizeT n)
    {
        return reserve(size() + n);
    }


    ////////////////////////////////////////////////////////////
    /// \brief Append copies of the `count` elements starting at `ptr` without growing
    ///
    /// The caller is responsible for ensuring that `size() + count <= capacity()`.
    ///
    ////////////////////////////////////////////////////////////
    [[gnu::always_inline, gnu::flatten]] void unsafeEmplaceBackRange(const TItem* const ptr, const SizeT count)
    {
        ZA_ASSERT(size() + count <= capacity());
        ZA_ASSERT(count == 0u || ptr != nullptr);

        if (count != 0u) // avoid `memcpy(null, null, 0)` (UB) when appending nothing
            priv::VectorUtils::copyRange(m_endSize, ptr, ptr + count);

        m_endSize += count;
    }


    ////////////////////////////////////////////////////////////
    /// \brief Destroy all elements; capacity is unchanged
    ///
    ////////////////////////////////////////////////////////////
    [[gnu::always_inline, gnu::flatten]] void clear() noexcept
    {
        priv::VectorUtils::destroyRange(m_data, m_endSize);
        m_endSize = m_data;
    }


    ////////////////////////////////////////////////////////////
    /// \brief Get the number of constructed elements
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] SizeT size() const noexcept
    {
        return static_cast<SizeT>(m_endSize - m_data);
    }


    ////////////////////////////////////////////////////////////
    /// \brief Get the number of elements the storage can hold without reallocating
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] SizeT capacity() const noexcept
    {
        return static_cast<SizeT>(m_endCapacity - m_data);
    }


    ////////////////////////////////////////////////////////////
    /// \brief `emplaceBack` without growing
    ///
    /// The caller is responsible for ensuring `size() < capacity()`.
    ///
    ////////////////////////////////////////////////////////////
    template <typename... Ts>
    [[gnu::always_inline]] TItem& unsafeEmplaceBack(Ts&&... xs)
    {
        ZA_ASSERT(m_endSize < m_endCapacity);

        // Size is only increased after a successful construction (no destruction of an unconstructed slot on throw)
        TItem& result = *(ZA_PLACEMENT_NEW(m_endSize) TItem(static_cast<Ts&&>(xs)...));
        ++m_endSize;

        return result;
    }


    ////////////////////////////////////////////////////////////
    /// \brief Erase the element at `it`, shifting subsequent elements left
    ///
    /// \return Iterator to the element that now occupies `it`'s position
    ///
    ////////////////////////////////////////////////////////////
    TItem* erase(const TItem* const it)
    {
        ZA_ASSERT(it >= begin() && it < end());

        TItem* const pos = m_data + (it - m_data);
        m_endSize        = priv::VectorUtils::eraseImpl(m_endSize, pos);

        return pos;
    }


    ////////////////////////////////////////////////////////////
    /// \brief Erase the half-open range `[first, last)`, shifting subsequent elements left
    ///
    /// \return Iterator to the element that now occupies `first`'s position
    ///
    ////////////////////////////////////////////////////////////
    TItem* erase(const TItem* const first, const TItem* const last)
    {
        ZA_ASSERT(first >= begin() && first <= last && last <= end());

        TItem* const pos = m_data + (first - m_data);

        if (first == last)
            return pos; // No elements to erase

        m_endSize = priv::VectorUtils::eraseRangeImpl(m_endSize, pos, m_data + (last - m_data));

        // Elements were shifted into `pos`, or it is the new `end()`.
        return pos;
    }


    ////////////////////////////////////////////////////////////
    /// \brief Append several elements at the back without growing
    ///
    /// The caller is responsible for ensuring `size() + sizeof...(items) <= capacity()`.
    ///
    ////////////////////////////////////////////////////////////
    template <typename... TItems>
    [[gnu::always_inline]] void unsafePushBackMultiple(TItems&&... items)
    {
        ZA_ASSERT(size() + sizeof...(items) <= capacity());

        (..., (ZA_PLACEMENT_NEW(m_endSize) TItem(static_cast<TItems&&>(items)), ++m_endSize));
    }


    ////////////////////////////////////////////////////////////
    /// \brief Pointer to the underlying contiguous storage (or `nullptr` if empty and unallocated)
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::pure]] TItem* data() noexcept ZA_LIFETIMEBOUND
    {
        return m_data;
    }


    ////////////////////////////////////////////////////////////
    /// \brief Pointer to the underlying contiguous storage (const overload)
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::pure]] const TItem* data() const noexcept ZA_LIFETIMEBOUND
    {
        return m_data;
    }


    ////////////////////////////////////////////////////////////
    /// \brief Force the logical size to `newSize` without constructing or destroying elements
    ///
    /// For storage filled by external code (`memcpy`, I/O); caller must ensure `newSize <= capacity()`.
    ///
    ////////////////////////////////////////////////////////////
    [[gnu::always_inline, gnu::flatten]] void unsafeSetSize(SizeT newSize) noexcept
    {
        ZA_ASSERT(newSize <= capacity());
        m_endSize = m_data + newSize;
    }


    ////////////////////////////////////////////////////////////
    /// \brief Destroy the last element; asserts that the vector is non-empty
    ///
    ////////////////////////////////////////////////////////////
    [[gnu::always_inline]] void popBack() noexcept
    {
        ZA_ASSERT(!empty());
        --m_endSize;

        if constexpr (!ZA_IS_TRIVIALLY_DESTRUCTIBLE(TItem))
            m_endSize->~TItem();
    }


    ////////////////////////////////////////////////////////////
    /// \brief Swap the contents of two vectors in O(1)
    ///
    ////////////////////////////////////////////////////////////
    [[gnu::always_inline]] void swap(Vector& rhs) noexcept
    {
        TItem* const tmpData        = m_data;
        TItem* const tmpEndSize     = m_endSize;
        TItem* const tmpEndCapacity = m_endCapacity;

        m_data        = rhs.m_data;
        m_endSize     = rhs.m_endSize;
        m_endCapacity = rhs.m_endCapacity;

        rhs.m_data        = tmpData;
        rhs.m_endSize     = tmpEndSize;
        rhs.m_endCapacity = tmpEndCapacity;
    }


    ////////////////////////////////////////////////////////////
    /// \brief Reference to the first element; asserts that the vector is non-empty
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::pure]] TItem& front() noexcept ZA_LIFETIMEBOUND
    {
        ZA_ASSERT(!empty());
        return *m_data;
    }


    ////////////////////////////////////////////////////////////
    /// \brief Reference to the first element (const overload)
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::pure]] const TItem& front() const noexcept ZA_LIFETIMEBOUND
    {
        ZA_ASSERT(!empty());
        return *m_data;
    }


    ////////////////////////////////////////////////////////////
    /// \brief Reference to the last element; asserts that the vector is non-empty
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::pure]] TItem& back() noexcept ZA_LIFETIMEBOUND
    {
        ZA_ASSERT(!empty());
        return *(m_endSize - 1u);
    }


    ////////////////////////////////////////////////////////////
    /// \brief Reference to the last element (const overload)
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::pure]] const TItem& back() const noexcept ZA_LIFETIMEBOUND
    {
        ZA_ASSERT(!empty());
        return *(m_endSize - 1u);
    }


    ////////////////////////////////////////////////////////////
    ZA_PRIV_DEFINE_COMMON_VECTOR_OPERATIONS(Vector);
};

} // namespace za
