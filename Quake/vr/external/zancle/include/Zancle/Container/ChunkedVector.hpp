#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include "Zancle/Container/Priv/VectorUtils.hpp"

#include "Zancle/Base/Assert.hpp"
#include "Zancle/Base/InitializerList.hpp" // IWYU pragma: keep
#include "Zancle/Base/LifetimeAttributes.hpp"
#include "Zancle/Base/PlacementNew.hpp"
#include "Zancle/Base/PtrDiffT.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Base/Swap.hpp"

#include "Zancle/Trait/Conditional.hpp"
#include "Zancle/Trait/EnableTrivialRelocation.hpp"
#include "Zancle/Trait/IsReference.hpp"
#include "Zancle/Trait/IsTriviallyDestructible.hpp"


namespace za::priv
{
////////////////////////////////////////////////////////////
/// \brief Default `BlockShift` for `ChunkedVector`
///
/// Returns the largest `shift` such that a block of `2^shift` items of
/// `itemSize` bytes fits in 64 KiB (or `0` if a single item is larger).
///
////////////////////////////////////////////////////////////
[[nodiscard]] consteval SizeT chunkedVectorDefaultBlockShift(const SizeT itemSize) noexcept
{
    SizeT shift = 0u;

    while ((SizeT{2u} << shift) * itemSize <= SizeT{65'536u})
        ++shift;

    return shift;
}

} // namespace za::priv


namespace za
{
////////////////////////////////////////////////////////////
/// \brief Block-allocated dynamic array with stable element addresses
///
/// ## Layout
///
/// A growable directory (`TItem**`) of fixed-size contiguous blocks.
/// Each block holds `2^BlockShift` elements. By default, `BlockShift` is
/// chosen so that a block occupies (at most) 64 KiB, e.g. 16 384 elements
/// for a 4-byte `TItem`. The default requires `TItem` to be complete:
/// specify `BlockShift` explicitly to use an incomplete `TItem`.
///
///     directory [ blk0* | blk1* | blk2* | ... ]
///                  |        |        |
///                  v        v        v
///               [items]  [items]  [items]   (each block is contiguous)
///
/// `operator[]` splits an index into a block index (high bits) and an
/// intra-block offset (low bits) via shift and mask -- O(1).
///
/// ## Growth
///
/// `pushBack`/`emplaceBack` allocate a new block when the current one
/// fills up, and geometrically grow the directory when it runs out of
/// slots. Existing elements are never relocated, so pointers/references
/// to elements remain valid across insertions. Consequently, inserting
/// or resizing from a reference to an existing element (e.g.
/// `resize(n, v[0])` or `pushBack(v[0])`) is supported.
///
///     pushBack / emplaceBack  -- amortized O(1)
///     operator[]              -- O(1)
///     reserve(n)              -- O(n / blockSize) block allocations
///
/// ## Iteration
///
/// The callback-based API (`forEach`, `forEachIndexed`, `forEachBlock`,
/// `findIf`) loops over each block as a contiguous span, giving the
/// compiler the same optimization opportunities as a flat-array traversal.
///
/// Random-access iterators are also provided. They cache a pointer to the
/// current element: dereferencing is a plain pointer dereference, and
/// `++`/`--` are a pointer bump that only consults the directory when
/// crossing a block boundary. Arbitrary jumps (`+=`, `-=`, ...) recompute
/// the element pointer through the directory.
///
/// Iterators refer to the container object (not only to its elements):
/// they are invalidated by moving from/into or swapping the container,
/// even though element addresses stay stable. Iterators to elements stay
/// valid across insertions and `reserve`/`shrinkToFit`, but the
/// past-the-end iterator is invalidated by any operation that changes
/// the size or the capacity.
///
////////////////////////////////////////////////////////////
template <typename TItem, SizeT BlockShift = priv::chunkedVectorDefaultBlockShift(sizeof(TItem))>
class [[nodiscard]] ZA_GSL_OWNER(TItem) ChunkedVector
{
    static_assert(BlockShift < sizeof(SizeT) * 8u);

private:
    ////////////////////////////////////////////////////////////
    TItem** m_directory{nullptr};    //!< Array of block pointers (length: m_directoryCapacity, used: m_numBlocks)
    SizeT   m_size{0u};              //!< Number of live (constructed) elements
    SizeT   m_numBlocks{0u};         //!< Number of allocated blocks (<= m_directoryCapacity)
    SizeT   m_directoryCapacity{0u}; //!< Allocated slots in the directory array


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline]] static constexpr SizeT sizeToBlockCount(const SizeT n) noexcept
    {
        ZA_ASSERT(n <= static_cast<SizeT>(-1) - blockMask); // `n + blockMask` must not wrap around
        return (n + blockMask) >> blockShift;
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::const]] static constexpr SizeT minSizeT(const SizeT lhs, const SizeT rhs) noexcept
    {
        return lhs < rhs ? lhs : rhs;
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::pure]] TItem* slotPtrUnchecked(const SizeT index) noexcept
    {
        return m_directory[index >> blockShift] + (index & blockMask);
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::pure]] const TItem* slotPtrUnchecked(const SizeT index) const noexcept
    {
        return m_directory[index >> blockShift] + (index & blockMask);
    }


    ////////////////////////////////////////////////////////////
    /// \brief Pointer to slot `index`, or `nullptr` if its block is not allocated
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] TItem* slotPtrOrNull(const SizeT index) noexcept
    {
        return index < capacity() ? slotPtrUnchecked(index) : nullptr;
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] const TItem* slotPtrOrNull(const SizeT index) const noexcept
    {
        return index < capacity() ? slotPtrUnchecked(index) : nullptr;
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard]] static TItem* allocateBlock()
    {
        // Here rather than in the class body, so that the class can be instantiated with an incomplete `TItem`
        static_assert(blockSize <= static_cast<SizeT>(-1) / sizeof(TItem), "Block byte size overflows `SizeT`");

        return priv::VectorUtils::allocate<TItem>(blockSize);
    }


    ////////////////////////////////////////////////////////////
    static void deallocateBlock(TItem* const p) noexcept
    {
        priv::VectorUtils::deallocate(p, blockSize);
    }


    ////////////////////////////////////////////////////////////
    void setDirectoryCapacityExact(const SizeT targetCapacity)
    {
        if (targetCapacity == m_directoryCapacity)
            return;

        ZA_ASSERT(targetCapacity >= m_numBlocks);

        if (targetCapacity == 0u)
        {
            priv::VectorUtils::deallocate(m_directory, m_directoryCapacity);
            m_directory         = nullptr;
            m_directoryCapacity = 0u;
            return;
        }

        auto** const newDirectory = priv::VectorUtils::allocate<TItem*>(targetCapacity);

        if (m_numBlocks > 0u)
            priv::VectorUtils::copyRange(newDirectory, m_directory, m_directory + m_numBlocks);

        priv::VectorUtils::deallocate(m_directory, m_directoryCapacity);

        m_directory         = newDirectory;
        m_directoryCapacity = targetCapacity;
    }


    ////////////////////////////////////////////////////////////
    [[gnu::cold, gnu::noinline, gnu::flatten]] void growDirectory(const SizeT targetBlockCount)
    {
        const auto currentCapacity       = m_directoryCapacity;
        const auto geometricGrowthTarget = currentCapacity == 0u ? SizeT{4u} : currentCapacity + (currentCapacity / 2u);
        const auto finalNewCapacity = targetBlockCount > geometricGrowthTarget ? targetBlockCount : geometricGrowthTarget;

        ZA_ASSERT(finalNewCapacity > m_directoryCapacity);
        setDirectoryCapacityExact(finalNewCapacity);
    }


    ////////////////////////////////////////////////////////////
    /// \brief Allocate blocks (and grow the directory) until `capacity() >= targetCapacity`
    ///
    /// Kept out of line so that the hot paths (`emplaceBack`, `reserve`)
    /// only contain a capacity check.
    ///
    ////////////////////////////////////////////////////////////
    [[gnu::cold, gnu::noinline]] void reserveImpl(const SizeT targetCapacity)
    {
        const SizeT targetBlockCount = sizeToBlockCount(targetCapacity);
        ZA_ASSERT(targetBlockCount > m_numBlocks); // Should only be called to grow

        if (m_directoryCapacity < targetBlockCount)
            growDirectory(targetBlockCount);

        while (m_numBlocks < targetBlockCount)
        {
            m_directory[m_numBlocks] = allocateBlock();
            ++m_numBlocks;
        }
    }


    ////////////////////////////////////////////////////////////
    /// \brief Invoke `fn(ptr, count)` for each contiguous run of slots in `[first, last)`
    ///
    /// Each run lies within a single block, so `fn` is invoked once per
    /// block. Iteration stops early as soon as `fn` returns `false`.
    ///
    /// \return `false` if the iteration was stopped early, `true` otherwise
    ///
    ////////////////////////////////////////////////////////////
    template <typename F>
    [[gnu::always_inline]] bool forEachChunk(this auto& self, SizeT first, const SizeT last, F&& fn)
    {
        while (first < last)
        {
            const SizeT count = minSizeT(last - first, blockSize - (first & blockMask));

            if (!fn(self.slotPtrUnchecked(first), count))
                return false;

            first += count;
        }

        return true;
    }


    ////////////////////////////////////////////////////////////
    void destroyIndexRange(const SizeT first, const SizeT last) noexcept
    {
        if constexpr (!ZA_IS_TRIVIALLY_DESTRUCTIBLE(TItem))
            forEachChunk(first,
                         last,
                         [](TItem* const ptr, const SizeT count)
            {
                priv::VectorUtils::destroyRange(ptr, ptr + count);
                return true;
            });
    }


    ////////////////////////////////////////////////////////////
    void copyFromContiguousRange(const SizeT targetIndex, const TItem* src, const SizeT count)
    {
        forEachChunk(targetIndex,
                     targetIndex + count,
                     [&](TItem* const ptr, const SizeT chunk)
        {
            priv::VectorUtils::copyRange(ptr, src, src + chunk);
            src += chunk;
            return true;
        });
    }


    ////////////////////////////////////////////////////////////
    void copyFromOther(const ChunkedVector& rhs)
    {
        ZA_ASSERT(m_size == 0u);
        reserve(rhs.m_size);

        // Copy block by block: `m_size` is bumped after each block, so it never counts unconstructed elements
        rhs.forEachChunk(0u,
                         rhs.m_size,
                         [&](const TItem* const ptr, const SizeT count)
        {
            unsafeEmplaceBackRange(ptr, count);
            return true;
        });
    }


    ////////////////////////////////////////////////////////////
    template <typename... Ts>
    void constructRange(const SizeT first, const SizeT last, Ts&&... xs)
    {
        forEachChunk(first,
                     last,
                     [&](TItem* const ptr, const SizeT count)
        {
            for (SizeT i = 0u; i < count; ++i)
                ZA_PLACEMENT_NEW(ptr + i) TItem(xs...); // intentionally not forwarding

            return true;
        });
    }


    ////////////////////////////////////////////////////////////
    /// \brief Destroy all elements and free all blocks and the directory
    ///
    /// Leaves the data members dangling: the caller must reassign them.
    ///
    ////////////////////////////////////////////////////////////
    void releaseStorage() noexcept
    {
        destroyIndexRange(0u, m_size);

        for (SizeT i = 0u; i < m_numBlocks; ++i)
            deallocateBlock(m_directory[i]);

        priv::VectorUtils::deallocate(m_directory, m_directoryCapacity);
    }


public:
    ////////////////////////////////////////////////////////////
    ZA_ENABLE_TRIVIAL_RELOCATION;


    ////////////////////////////////////////////////////////////
    static constexpr SizeT blockShift = BlockShift;
    static constexpr SizeT blockSize  = SizeT{1u} << BlockShift;
    static constexpr SizeT blockMask  = blockSize - 1u;


    ////////////////////////////////////////////////////////////
    using value_type      = TItem;
    using pointer         = TItem*;
    using const_pointer   = const TItem*;
    using reference       = TItem&;
    using const_reference = const TItem&;
    using size_type       = SizeT;
    using difference_type = PtrDiffT;


    ////////////////////////////////////////////////////////////
    /// \brief Random-access iterator caching a pointer to the current element
    ///
    /// Invariant: `m_ptr` points to slot `m_index` of `*m_owner`, or is
    /// `nullptr` if that slot's block is not allocated (only possible for
    /// the past-the-end position when `size() == capacity()`).
    ///
    /// Comparisons and differences are hidden friends: mixing mutable
    /// and const iterators works in both directions through the implicit
    /// mutable-to-const conversion.
    ///
    ////////////////////////////////////////////////////////////
    template <bool IsConst>
    class IteratorImpl
    {
        friend IteratorImpl<!IsConst>;

    public:
        using value_type      = TItem;
        using pointer         = Conditional<IsConst, const TItem*, TItem*>;
        using reference       = Conditional<IsConst, const TItem&, TItem&>;
        using difference_type = PtrDiffT;
        using owner_pointer   = Conditional<IsConst, const ChunkedVector*, ChunkedVector*>;

    private:
        owner_pointer m_owner{nullptr};
        pointer       m_ptr{nullptr};
        SizeT         m_index{0u};


        ////////////////////////////////////////////////////////////
        [[gnu::always_inline]] void reloadPtr() noexcept
        {
            m_ptr = m_owner->slotPtrOrNull(m_index);
        }

    public:
        [[nodiscard]] IteratorImpl()                    = default;
        [[nodiscard]] IteratorImpl(const IteratorImpl&) = default;
        IteratorImpl& operator=(const IteratorImpl&)    = default;


        [[nodiscard, gnu::always_inline]] IteratorImpl(const IteratorImpl<false>& rhs) noexcept
            requires(IsConst)
            : m_owner{rhs.m_owner}, m_ptr{rhs.m_ptr}, m_index{rhs.m_index}
        {
        }


        [[nodiscard, gnu::always_inline]] IteratorImpl(owner_pointer owner, const SizeT index) noexcept :
            m_owner{owner},
            m_ptr{owner->slotPtrOrNull(index)},
            m_index{index}
        {
        }


        [[nodiscard, gnu::always_inline]] reference operator*() const noexcept
        {
            return *m_ptr;
        }


        [[nodiscard, gnu::always_inline]] pointer operator->() const noexcept
        {
            return m_ptr;
        }


        [[nodiscard, gnu::always_inline]] reference operator[](const difference_type delta) const noexcept
        {
            return (*m_owner)[static_cast<SizeT>(static_cast<difference_type>(m_index) + delta)];
        }


        [[gnu::always_inline]] IteratorImpl& operator++() noexcept
        {
            ++m_index;

            if ((m_index & blockMask) == 0u) [[unlikely]] // Entered the next block
                reloadPtr();
            else
                ++m_ptr;

            return *this;
        }


        [[gnu::always_inline]] IteratorImpl operator++(int) noexcept
        {
            const IteratorImpl result = *this;
            ++*this;
            return result;
        }


        [[gnu::always_inline]] IteratorImpl& operator--() noexcept
        {
            if ((m_index & blockMask) == 0u) [[unlikely]] // Leaving the current block
            {
                --m_index;
                reloadPtr();
            }
            else
            {
                --m_index;
                --m_ptr;
            }

            return *this;
        }


        [[gnu::always_inline]] IteratorImpl operator--(int) noexcept
        {
            const IteratorImpl result = *this;
            --*this;
            return result;
        }


        [[gnu::always_inline]] IteratorImpl& operator+=(const difference_type delta) noexcept
        {
            m_index = static_cast<SizeT>(static_cast<difference_type>(m_index) + delta);
            reloadPtr();
            return *this;
        }


        [[gnu::always_inline]] IteratorImpl& operator-=(const difference_type delta) noexcept
        {
            return *this += -delta;
        }


        [[nodiscard, gnu::always_inline]] friend IteratorImpl operator+(IteratorImpl it, const difference_type delta) noexcept
        {
            it += delta;
            return it;
        }


        [[nodiscard, gnu::always_inline]] friend IteratorImpl operator+(const difference_type delta, IteratorImpl it) noexcept
        {
            it += delta;
            return it;
        }


        [[nodiscard, gnu::always_inline]] friend IteratorImpl operator-(IteratorImpl it, const difference_type delta) noexcept
        {
            it -= delta;
            return it;
        }


        [[nodiscard, gnu::always_inline]] friend difference_type operator-(const IteratorImpl& lhs, const IteratorImpl& rhs) noexcept
        {
            ZA_ASSERT(lhs.m_owner == rhs.m_owner);
            return static_cast<difference_type>(lhs.m_index) - static_cast<difference_type>(rhs.m_index);
        }


        [[nodiscard, gnu::always_inline]] friend bool operator==(const IteratorImpl& lhs, const IteratorImpl& rhs) noexcept
        {
            ZA_ASSERT(lhs.m_owner == rhs.m_owner);
            return lhs.m_index == rhs.m_index;
        }


        [[nodiscard, gnu::always_inline]] friend bool operator<(const IteratorImpl& lhs, const IteratorImpl& rhs) noexcept
        {
            ZA_ASSERT(lhs.m_owner == rhs.m_owner);
            return lhs.m_index < rhs.m_index;
        }


        [[nodiscard, gnu::always_inline]] friend bool operator<=(const IteratorImpl& lhs, const IteratorImpl& rhs) noexcept
        {
            return !(rhs < lhs);
        }


        [[nodiscard, gnu::always_inline]] friend bool operator>(const IteratorImpl& lhs, const IteratorImpl& rhs) noexcept
        {
            return rhs < lhs;
        }


        [[nodiscard, gnu::always_inline]] friend bool operator>=(const IteratorImpl& lhs, const IteratorImpl& rhs) noexcept
        {
            return !(lhs < rhs);
        }
    };


    ////////////////////////////////////////////////////////////
    using Iterator       = IteratorImpl<false>;
    using ConstIterator  = IteratorImpl<true>;
    using iterator       = Iterator;
    using const_iterator = ConstIterator;


    ////////////////////////////////////////////////////////////
    [[nodiscard]] ChunkedVector() = default;


    ////////////////////////////////////////////////////////////
    // Note: the non-default constructors delegate to the default one, so that
    // the destructor releases the allocated storage if their body throws.
    ////////////////////////////////////////////////////////////


    ////////////////////////////////////////////////////////////
    [[nodiscard]] explicit ChunkedVector(const SizeT initialSize) : ChunkedVector()
    {
        resize(initialSize);
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard]] explicit ChunkedVector(const SizeT initialSize, const TItem& value) : ChunkedVector()
    {
        resize(initialSize, value);
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard]] explicit ChunkedVector(const TItem* const srcBegin, const TItem* const srcEnd) : ChunkedVector()
    {
        ZA_ASSERT(srcBegin <= srcEnd);
        const auto srcCount = static_cast<SizeT>(srcEnd - srcBegin);

        reserve(srcCount);
        unsafeEmplaceBackRange(srcBegin, srcCount);
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard]] /* implicit */ ChunkedVector(const std::initializer_list<TItem> iList) :
        ChunkedVector(iList.begin(), iList.end())
    {
    }


    ////////////////////////////////////////////////////////////
    ~ChunkedVector()
    {
        releaseStorage();
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard]] ChunkedVector(const ChunkedVector& rhs) : ChunkedVector()
    {
        copyFromOther(rhs);
    }


    ////////////////////////////////////////////////////////////
    ChunkedVector& operator=(const ChunkedVector& rhs)
    {
        if (this == &rhs)
            return *this;

        clear();
        copyFromOther(rhs);

        return *this;
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline]] ChunkedVector(ChunkedVector&& rhs) noexcept :
        m_directory{rhs.m_directory},
        m_size{rhs.m_size},
        m_numBlocks{rhs.m_numBlocks},
        m_directoryCapacity{rhs.m_directoryCapacity}
    {
        rhs.m_directory         = nullptr;
        rhs.m_size              = 0u;
        rhs.m_numBlocks         = 0u;
        rhs.m_directoryCapacity = 0u;
    }


    ////////////////////////////////////////////////////////////
    ChunkedVector& operator=(ChunkedVector&& rhs) noexcept
    {
        if (this == &rhs)
            return *this;

        releaseStorage();

        m_directory         = rhs.m_directory;
        m_size              = rhs.m_size;
        m_numBlocks         = rhs.m_numBlocks;
        m_directoryCapacity = rhs.m_directoryCapacity;

        rhs.m_directory         = nullptr;
        rhs.m_size              = 0u;
        rhs.m_numBlocks         = 0u;
        rhs.m_directoryCapacity = 0u;

        return *this;
    }


    ////////////////////////////////////////////////////////////
    /// \brief Resize to `newSize`, constructing new elements from `args...`
    ///
    /// `args...` are passed as lvalues to every new element's constructor.
    /// They may refer to existing elements (e.g. `resize(n, v[0])`), as
    /// growing never relocates existing elements.
    ///
    ////////////////////////////////////////////////////////////
    void resize(const SizeT newSize, auto&&... args)
    {
        const auto oldSize = m_size;

        if (newSize > oldSize)
        {
            reserve(newSize);
            constructRange(oldSize, newSize, args...); // intentionally not forwarding
        }
        else if (newSize < oldSize)
        {
            destroyIndexRange(newSize, oldSize);
        }

        m_size = newSize;
    }


    ////////////////////////////////////////////////////////////
    template <typename T = TItem>
    [[gnu::always_inline, gnu::flatten]] TItem& pushBack(T&& x)
    {
        return emplaceBack(static_cast<T&&>(x));
    }


    ////////////////////////////////////////////////////////////
    template <typename... Ts>
    [[gnu::always_inline, gnu::flatten]] TItem& emplaceBack(Ts&&... xs)
    {
        if (m_size == capacity()) [[unlikely]]
            reserveImpl(m_size + 1u);

        // Construct before bumping the size, so that a throwing constructor leaves the size unchanged
        TItem& result = *(ZA_PLACEMENT_NEW(slotPtrUnchecked(m_size)) TItem(static_cast<Ts&&>(xs)...));
        ++m_size;

        return result;
    }


    ////////////////////////////////////////////////////////////
    void shrinkToFit()
    {
        const SizeT requiredBlocks = sizeToBlockCount(m_size);

        if (requiredBlocks < m_numBlocks)
        {
            for (SizeT blockIndex = requiredBlocks; blockIndex < m_numBlocks; ++blockIndex)
                deallocateBlock(m_directory[blockIndex]);

            m_numBlocks = requiredBlocks;
        }

        setDirectoryCapacityExact(requiredBlocks);
    }


    ////////////////////////////////////////////////////////////
    [[gnu::always_inline]] void reserve(const SizeT targetCapacity)
    {
        if (capacity() < targetCapacity) [[unlikely]]
            reserveImpl(targetCapacity);
    }


    ////////////////////////////////////////////////////////////
    [[gnu::always_inline]] void reserveMore(const SizeT n)
    {
        reserve(m_size + n);
    }


    ////////////////////////////////////////////////////////////
    [[gnu::always_inline]] void unsafeEmplaceBackRange(const TItem* const ptr, const SizeT count)
    {
        ZA_ASSERT(count == 0u || ptr != nullptr);
        ZA_ASSERT(m_size + count <= capacity());

        copyFromContiguousRange(m_size, ptr, count);
        m_size += count;
    }


    ////////////////////////////////////////////////////////////
    [[gnu::always_inline]] void clear() noexcept
    {
        destroyIndexRange(0u, m_size);
        m_size = 0u;
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] SizeT size() const noexcept
    {
        return m_size;
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] SizeT capacity() const noexcept
    {
        return m_numBlocks * blockSize;
    }


    ////////////////////////////////////////////////////////////
    template <typename... Ts>
    [[gnu::always_inline]] TItem& unsafeEmplaceBack(Ts&&... xs)
    {
        ZA_ASSERT(m_size < capacity());

        // Construct before bumping the size, so that a throwing constructor leaves the size unchanged
        TItem& result = *(ZA_PLACEMENT_NEW(slotPtrUnchecked(m_size)) TItem(static_cast<Ts&&>(xs)...));
        ++m_size;

        return result;
    }


    ////////////////////////////////////////////////////////////
    template <typename... TItems>
    [[gnu::always_inline]] void unsafePushBackMultiple(TItems&&... items)
    {
        ZA_ASSERT(m_size + sizeof...(items) <= capacity());
        (..., unsafeEmplaceBack(static_cast<TItems&&>(items)));
    }


    ////////////////////////////////////////////////////////////
    [[gnu::always_inline, gnu::flatten]] void unsafeSetSize(const SizeT newSize) noexcept
    {
        ZA_ASSERT(newSize <= capacity());
        m_size = newSize;
    }


    ////////////////////////////////////////////////////////////
    [[gnu::always_inline]] void popBack() noexcept
    {
        ZA_ASSERT(!empty());
        --m_size;

        if constexpr (!ZA_IS_TRIVIALLY_DESTRUCTIBLE(TItem))
            slotPtrUnchecked(m_size)->~TItem();
    }


    ////////////////////////////////////////////////////////////
    [[gnu::always_inline]] void swap(ChunkedVector& rhs) noexcept
    {
        za::genericSwap(m_directory, rhs.m_directory);
        za::genericSwap(m_size, rhs.m_size);
        za::genericSwap(m_numBlocks, rhs.m_numBlocks);
        za::genericSwap(m_directoryCapacity, rhs.m_directoryCapacity);
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::pure]] TItem& operator[](const SizeT i) noexcept ZA_LIFETIMEBOUND
    {
        ZA_ASSERT(i < m_size);
        return *slotPtrUnchecked(i);
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::pure]] const TItem& operator[](const SizeT i) const noexcept ZA_LIFETIMEBOUND
    {
        ZA_ASSERT(i < m_size);
        return *slotPtrUnchecked(i);
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] iterator begin() noexcept ZA_LIFETIMEBOUND
    {
        return iterator{this, 0u};
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] const_iterator begin() const noexcept ZA_LIFETIMEBOUND
    {
        return const_iterator{this, 0u};
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] iterator end() noexcept ZA_LIFETIMEBOUND
    {
        return iterator{this, m_size};
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] const_iterator end() const noexcept ZA_LIFETIMEBOUND
    {
        return const_iterator{this, m_size};
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] const_iterator cbegin() const noexcept ZA_LIFETIMEBOUND
    {
        return begin();
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] const_iterator cend() const noexcept ZA_LIFETIMEBOUND
    {
        return end();
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] bool empty() const noexcept
    {
        return m_size == 0u;
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard]] bool operator==(const ChunkedVector& rhs) const
    {
        if (this == &rhs)
            return true;

        if (m_size != rhs.m_size)
            return false;

        SizeT index = 0u;

        return forEachChunk(0u,
                            m_size,
                            [&](const TItem* const lhsPtr, const SizeT count)
        {
            const TItem* const rhsPtr = rhs.slotPtrUnchecked(index);
            index += count;

            for (SizeT i = 0u; i < count; ++i)
                if (lhsPtr[i] != rhsPtr[i])
                    return false;

            return true;
        });
    }


    ////////////////////////////////////////////////////////////
    [[gnu::always_inline]] friend void swap(ChunkedVector& lhs, ChunkedVector& rhs) noexcept
    {
        lhs.swap(rhs);
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::pure]] TItem& front() noexcept ZA_LIFETIMEBOUND
    {
        ZA_ASSERT(!empty());
        return *slotPtrUnchecked(0u);
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::pure]] const TItem& front() const noexcept ZA_LIFETIMEBOUND
    {
        ZA_ASSERT(!empty());
        return *slotPtrUnchecked(0u);
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::pure]] TItem& back() noexcept ZA_LIFETIMEBOUND
    {
        ZA_ASSERT(!empty());
        return *slotPtrUnchecked(m_size - 1u);
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::pure]] const TItem& back() const noexcept ZA_LIFETIMEBOUND
    {
        ZA_ASSERT(!empty());
        return *slotPtrUnchecked(m_size - 1u);
    }


    ////////////////////////////////////////////////////////////
    /// \brief Invoke `fn(item)` for each element, in order
    ///
    ////////////////////////////////////////////////////////////
    template <typename F>
    [[gnu::always_inline]] void forEach(this auto&& self, F&& fn)
    {
        self.forEachChunk(0u,
                          self.m_size,
                          [&](auto* ptr, const SizeT count)
        {
            auto& f = fn; // Local alias: avoids reloading the capture per element in debug builds

            for (auto* const end = ptr + count; ptr != end; ++ptr)
                f(*ptr);

            return true;
        });
    }


    ////////////////////////////////////////////////////////////
    /// \brief Invoke `fn(index, item)` for each element, in order
    ///
    ////////////////////////////////////////////////////////////
    template <typename F>
    [[gnu::always_inline]] void forEachIndexed(this auto&& self, F&& fn)
    {
        SizeT baseIndex = 0u;

        self.forEachChunk(0u,
                          self.m_size,
                          [&](auto* const ptr, const SizeT count)
        {
            auto& f = fn; // Local alias: avoids reloading the capture per element in debug builds

            for (SizeT i = 0u; i < count; ++i)
                f(baseIndex + i, ptr[i]);

            baseIndex += count;
            return true;
        });
    }


    ////////////////////////////////////////////////////////////
    /// \brief Invoke `fn(blockBegin, blockEnd)` for each block's contiguous range of elements, in order
    ///
    ////////////////////////////////////////////////////////////
    template <typename F>
    [[gnu::always_inline]] void forEachBlock(this auto&& self, F&& fn)
    {
        self.forEachChunk(0u,
                          self.m_size,
                          [&](auto* const ptr, const SizeT count)
        {
            fn(ptr, ptr + count);
            return true;
        });
    }


    ////////////////////////////////////////////////////////////
    /// \brief Pointer to the first element satisfying `predicate`, or `nullptr`
    ///
    /// Only callable on lvalues: the result points into the container.
    ///
    ////////////////////////////////////////////////////////////
    template <typename TSelf, typename TPredicate>
        requires(za::isReference<TSelf>)
    [[nodiscard]] auto findIf(this TSelf&& self, TPredicate&& predicate) -> decltype(self.slotPtrUnchecked(0u))
    {
        decltype(self.slotPtrUnchecked(0u)) result = nullptr;

        self.forEachChunk(0u,
                          self.m_size,
                          [&](auto* ptr, const SizeT count)
        {
            auto& pred = predicate; // Local alias: avoids reloading the capture per element in debug builds

            for (auto* const end = ptr + count; ptr != end; ++ptr)
                if (pred(*ptr))
                {
                    result = ptr;
                    return false;
                }

            return true;
        });

        return result;
    }


    ////////////////////////////////////////////////////////////
    template <typename TResult, typename F>
    [[nodiscard]] TResult reduce(TResult init, F&& fn) const
    {
        forEach([&](const TItem& x) { init = fn(static_cast<TResult&&>(init), x); });
        return init;
    }
};

} // namespace za
