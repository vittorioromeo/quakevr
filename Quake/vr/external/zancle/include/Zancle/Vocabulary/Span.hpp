#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include "Zancle/Base/Assert.hpp"
#include "Zancle/Base/LifetimeAttributes.hpp"
#include "Zancle/Base/SizeT.hpp"

#include "Zancle/Trait/IsConvertible.hpp"
#include "Zancle/Trait/IsSame.hpp"
#include "Zancle/Trait/RemoveCVRef.hpp"


namespace za::priv
{
////////////////////////////////////////////////////////////
// `std::span`-style element compatibility: `U*` may be viewed as a span of
// `T` if `U(*)[]` converts to `T(*)[]`. This allows adding cv-qualifiers, but
// rejects e.g. derived-to-base conversions, which would break indexing.
template <typename UPtr, typename T>
inline constexpr bool isSpanCompatiblePtr = false;

template <typename U, typename T>
inline constexpr bool isSpanCompatiblePtr<U*, T> = isConvertible<U (*)[], T (*)[]>;

} // namespace za::priv


namespace za
{
////////////////////////////////////////////////////////////
/// \brief Non-owning view over a contiguous range of `T`
///
/// Lightweight `std::span` replacement. Stores a pointer plus a size,
/// implicitly converts from C arrays and any range-like type with
/// `data()` and `size()`. A `Span<T>` implicitly converts to a
/// `Span<const T>` for read-only views.
///
/// `Span` does not own its memory; the caller must ensure that the
/// referenced range outlives the span. Constructing a `Span` from a
/// temporary owning container (e.g. `Span<int> s = makeVector();`) is
/// diagnosed by Clang (`-Wdangling-gsl`), while passing one as a function
/// argument is fine.
///
////////////////////////////////////////////////////////////
template <typename T>
struct ZA_GSL_POINTER(T) Span
{
    ////////////////////////////////////////////////////////////
    /// \brief Default constructor, creates an empty span (`data = nullptr`, `size = 0`)
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline]] constexpr Span() = default;


    ////////////////////////////////////////////////////////////
    /// \brief Construct from pointer and size; `data` may be null only if `size == 0`
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline]] constexpr Span(T* data, SizeT size) : theData{data}, theSize{size}
    {
        ZA_ASSERT(theData != nullptr || theSize == 0u);
    }


    ////////////////////////////////////////////////////////////
    /// \brief Reject pointers to incompatible element types (e.g. derived-to-base)
    ///
    ////////////////////////////////////////////////////////////
    template <typename U>
        requires(!priv::isSpanCompatiblePtr<U*, T>)
    constexpr Span(U* data, SizeT size) = delete;


    ////////////////////////////////////////////////////////////
    /// \brief Construct a span from a C-style array (deduced size)
    ///
    ////////////////////////////////////////////////////////////
    template <SizeT N>
    [[nodiscard, gnu::always_inline]] constexpr Span(T (&array ZA_LIFETIMEBOUND)[N]) : theData{array}, theSize{N}
    {
    }


    ////////////////////////////////////////////////////////////
    /// \brief Construct a span from any contiguous range exposing `data()` and `size()`
    ///
    /// Disabled when `Range` is itself a `Span` so it does not shadow the
    /// implicitly generated copy constructor. Also provides the implicit
    /// conversion from `Span<T>` to `Span<const T>`.
    ///
    ////////////////////////////////////////////////////////////
    template <typename Range>
        requires(!isSame<RemoveCVRefIndirect<Range>, Span> &&
                 requires(Range&& r) {
                     requires priv::isSpanCompatiblePtr<decltype(r.data()), T>;
                     r.size();
                 })
    [[nodiscard, gnu::always_inline]] constexpr Span(Range&& range) : theData{range.data()}, theSize{range.size()}
    {
    }


    ////////////////////////////////////////////////////////////
    /// \brief Iterator to the first element
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] constexpr T* begin() const
    {
        return theData;
    }


    ////////////////////////////////////////////////////////////
    /// \brief Iterator one past the last element
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] constexpr T* end() const
    {
        return theData + theSize;
    }


    ////////////////////////////////////////////////////////////
    /// \brief Indexed element access (asserts `i < size()`)
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] constexpr T& operator[](SizeT i) const
    {
        ZA_ASSERT(i < theSize);
        return *(theData + i);
    }


    ////////////////////////////////////////////////////////////
    /// \brief Pointer to the first element (may be `nullptr` if empty)
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] constexpr T* data() const noexcept
    {
        return theData;
    }


    ////////////////////////////////////////////////////////////
    /// \brief Number of elements in the span
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] constexpr SizeT size() const noexcept
    {
        return theSize;
    }


    ////////////////////////////////////////////////////////////
    /// \brief Check whether the span has zero elements
    ///
    /// Note: returns `true` for both `(nullptr, 0)` and `(non-null, 0)`.
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] constexpr bool empty() const
    {
        return theSize == 0u;
    }


    ////////////////////////////////////////////////////////////
    /// \brief Check whether the span has zero elements *or* a null data pointer
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] constexpr bool isNullOrEmpty() const
    {
        return theData == nullptr || theSize == 0u;
    }


    ////////////////////////////////////////////////////////////
    /// \brief Size of the viewed elements in bytes (like `std::span::size_bytes`)
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] constexpr SizeT sizeBytes() const noexcept
    {
        return theSize * sizeof(T);
    }


    ////////////////////////////////////////////////////////////
    /// \brief First element (the span must not be empty)
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] constexpr T& front() const
    {
        ZA_ASSERT(theSize > 0u);
        return *theData;
    }


    ////////////////////////////////////////////////////////////
    /// \brief Last element (the span must not be empty)
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] constexpr T& back() const
    {
        ZA_ASSERT(theSize > 0u);
        return *(theData + theSize - 1u);
    }


    ////////////////////////////////////////////////////////////
    /// \brief View of the up to `len` elements starting at `startPos` (like `StringView::substrByPosLen`)
    ///
    /// `startPos` must be at most `size()`. Without `len` (or when it exceeds
    /// what is left), the view extends to the end of the span: unlike
    /// `std::span::subspan`, which requires the count to fit.
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] constexpr Span subspanByPosLen(const SizeT startPos,
                                                                                const SizeT len = static_cast<SizeT>(-1)) const
    {
        ZA_ASSERT(startPos <= theSize);

        const SizeT available = theSize - startPos;
        return Span{theData + startPos, len < available ? len : available};
    }


    ////////////////////////////////////////////////////////////
    /// \brief Element-wise comparison of two ranges
    ///
    /// Returns `true` only if both ranges have the same length and every
    /// pair of elements compares equal with `operator==`.
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] constexpr bool valueEquals(const T* rhsData, SizeT rhsSize) const
    {
        if (theSize != rhsSize)
            return false;

        for (SizeT i = 0u; i < theSize; ++i)
            if (theData[i] != rhsData[i])
                return false;

        return true;
    }


    ////////////////////////////////////////////////////////////
    /// \brief Element-wise comparison with another span
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] constexpr bool valueEquals(const Span& rhs) const
    {
        return valueEquals(rhs.theData, rhs.theSize);
    }


    ////////////////////////////////////////////////////////////
    // Member data
    ////////////////////////////////////////////////////////////
    T*    theData{nullptr};
    SizeT theSize{0u};
};

} // namespace za
