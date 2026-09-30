#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include "Zancle/Math/MinMaxMacros.hpp"

#include "Zancle/Base/Assert.hpp"
#include "Zancle/Base/LifetimeAttributes.hpp"
#include "Zancle/Base/Memcmp.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Base/Strlen.hpp"


namespace za
{
////////////////////////////////////////////////////////////
/// \brief Non-owning view over a contiguous sequence of `char`
///
/// Lightweight `std::string_view` replacement that avoids the heavy
/// `<string_view>` standard header. Provides the usual interface:
/// length-aware comparisons, `find*` family, prefix/suffix removal,
/// substring extraction, and friend operators against C strings.
///
/// `StringView` does not own its data; the caller must ensure the
/// referenced character buffer outlives the view. The implicit
/// constructor from `nullptr` is deleted to catch accidental misuse.
///
////////////////////////////////////////////////////////////
class ZA_GSL_POINTER(char) StringView
{
private:
    //////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] static constexpr bool containsChar(const char        needle,
                                                                                    const StringView& haystack) noexcept
    {
        for (char c : haystack)
            if (c == needle)
                return true;

        return false;
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] static constexpr int constexprMemCmp(const char* s1, const char* s2, SizeT n) noexcept
    {
        if consteval
        {
            for (SizeT i = 0; i < n; ++i)
                if (s1[i] != s2[i])
                    return static_cast<int>(static_cast<unsigned char>(s1[i])) -
                           static_cast<int>(static_cast<unsigned char>(s2[i]));

            return 0;
        }

        return ZA_MEMCMP(s1, s2, n);
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] static constexpr SizeT constexprStrLen(const char* const cStr) noexcept
    {
        if consteval
        {
            const char* end = cStr;

            while (*end != '\0')
                ++end;

            return static_cast<SizeT>(end - cStr);
        }

        return ZA_STRLEN(cStr);
    }


public:
    ////////////////////////////////////////////////////////////
    enum : SizeT
    {
        nPos = static_cast<SizeT>(-1)
    };


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline]] constexpr StringView(decltype(nullptr)) = delete;


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline]] constexpr /* implicit */ StringView() noexcept = default;


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline]] constexpr StringView(const char* const cStr) noexcept :
        theData{[cStr]
    {
        ZA_ASSERT(cStr != nullptr); // assert before strlen to avoid UB
        return cStr;
    }()},
        theSize{constexprStrLen(cStr)}
    {
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline]] constexpr StringView(const char* const cStr, const SizeT len) noexcept :
        theData{cStr},
        theSize{len}
    {
        ZA_ASSERT(cStr != nullptr || (cStr == nullptr && len == 0u));
    }


    ////////////////////////////////////////////////////////////
    template <typename StringLike>
    [[nodiscard, gnu::always_inline]] constexpr StringView(const StringLike& stringLike) noexcept
        requires(requires {
                    stringLike.data();
                    stringLike.size();
                })
        : theData{stringLike.data()}, theSize{stringLike.size()}
    {
        ZA_ASSERT(stringLike.data() != nullptr);
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] constexpr const char* data() const noexcept
    {
        return theData;
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] constexpr SizeT size() const noexcept
    {
        return theSize;
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] constexpr bool empty() const noexcept
    {
        return theSize == 0u;
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] constexpr StringView substrByPosLen(const SizeT startPos = 0u,
                                                                                     const SizeT len      = nPos) const
    {
        ZA_ASSERT(startPos <= theSize);

        const SizeT maxPossibleLength = startPos > theSize ? 0 : theSize - startPos;
        const SizeT lengthToUse       = ZA_MIN(len, maxPossibleLength);

        return {theData + startPos, lengthToUse};
    }


    ////////////////////////////////////////////////////////////
    [[gnu::always_inline]] constexpr void removePrefix(const SizeT n) noexcept
    {
        ZA_ASSERT(n <= theSize);
        theData += n;
        theSize -= n;
    }


    ////////////////////////////////////////////////////////////
    [[gnu::always_inline]] constexpr void removeSuffix(const SizeT n) noexcept
    {
        ZA_ASSERT(n <= theSize);
        theSize -= n;
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] constexpr SizeT find(const StringView v, const SizeT startPos = 0u) const noexcept
    {
        if (v.theSize == 0)
            return startPos <= theSize ? startPos : nPos;

        if (startPos >= theSize || v.theSize > theSize - startPos)
            return nPos;

        const char* const lastPossibleStart = theData + theSize - v.theSize;

        for (const char* p = theData + startPos; p <= lastPossibleStart; ++p)
            if (constexprMemCmp(p, v.theData, v.theSize) == 0)
                return static_cast<SizeT>(p - theData);

        return nPos;
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] constexpr SizeT find(const char c, const SizeT startPos = 0u) const noexcept
    {
        if (startPos >= theSize)
            return nPos;

        for (SizeT i = startPos; i < theSize; ++i)
            if (theData[i] == c)
                return i;

        return nPos;
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] constexpr SizeT rfind(const StringView v, const SizeT startPos = nPos) const noexcept
    {
        if (v.theSize > theSize)
            return nPos;

        if (v.empty())
            return ZA_MIN(startPos, theSize);

        SizeT pos = ZA_MIN(startPos, theSize - v.theSize);

        do
        {
            if (constexprMemCmp(theData + pos, v.theData, v.theSize) == 0)
                return pos;
        } while (pos-- > 0);

        return nPos;
    }

    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] constexpr SizeT rfind(const char c, const SizeT startPos = nPos) const noexcept
    {
        if (empty())
            return nPos;

        SizeT pos = ZA_MIN(startPos, theSize - 1);

        do
        {
            if (theData[pos] == c)
                return pos;
        } while (pos-- > 0);

        return nPos;
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] constexpr SizeT findFirstOf(const StringView v,
                                                                             const SizeT startPos = 0u) const noexcept
    {
        const SizeT maxIdx = theSize;

        for (SizeT i = startPos; i < maxIdx; ++i)
            if (containsChar(theData[i], v))
                return i;

        return nPos;
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] constexpr SizeT findFirstOf(const char c, const SizeT startPos = 0u) const noexcept
    {
        return find(c, startPos);
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] constexpr SizeT findLastOf(const StringView v,
                                                                            const SizeT startPos = nPos) const noexcept
    {
        if (empty())
            return nPos;

        SizeT pos = ZA_MIN(startPos, theSize - 1u);

        do
        {
            if (containsChar(theData[pos], v))
                return pos;
        } while (pos-- > 0u);

        return nPos;
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] constexpr SizeT findLastOf(const char c, const SizeT startPos = nPos) const noexcept
    {
        return rfind(c, startPos);
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] constexpr SizeT findFirstNotOf(const StringView v,
                                                                                const SizeT startPos = 0u) const noexcept
    {
        const SizeT maxIdx = theSize;

        for (SizeT i = startPos; i < maxIdx; ++i)
            if (!containsChar(theData[i], v))
                return i;

        return nPos;
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] constexpr SizeT findFirstNotOf(const char c, const SizeT startPos = 0u) const noexcept
    {
        for (SizeT i = startPos; i < theSize; ++i)
            if (theData[i] != c)
                return i;

        return nPos;
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] constexpr SizeT findLastNotOf(const StringView v,
                                                                               const SizeT startPos = nPos) const noexcept
    {
        if (empty())
            return nPos;

        SizeT pos = ZA_MIN(startPos, theSize - 1u);

        do
        {
            if (!containsChar(theData[pos], v))
                return pos;
        } while (pos-- > 0u);

        return nPos;
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] constexpr SizeT findLastNotOf(const char c, const SizeT startPos = nPos) const noexcept
    {
        if (empty())
            return nPos;

        SizeT pos = ZA_MIN(startPos, theSize - 1u);

        do
        {
            if (theData[pos] != c)
                return pos;
        } while (pos-- > 0u);

        return nPos;
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] constexpr bool startsWith(const StringView prefix) const noexcept
    {
        return theSize >= prefix.theSize && constexprMemCmp(theData, prefix.theData, prefix.theSize) == 0;
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] constexpr bool startsWith(const char c) const noexcept
    {
        return theSize > 0u && theData[0] == c;
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] constexpr bool endsWith(const StringView suffix) const noexcept
    {
        return theSize >= suffix.theSize &&
               constexprMemCmp(theData + theSize - suffix.theSize, suffix.theData, suffix.theSize) == 0;
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] constexpr bool endsWith(const char c) const noexcept
    {
        return theSize > 0u && theData[theSize - 1u] == c;
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] constexpr bool contains(const StringView needle) const noexcept
    {
        return find(needle) != nPos;
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] constexpr bool contains(const char c) const noexcept
    {
        return find(c) != nPos;
    }


private:
    template <typename Splitter, typename F>
    constexpr void forSplitsImpl(Splitter splitter, F&& f) const;


public:
    ////////////////////////////////////////////////////////////
    /// \brief Invoke `f(segment)` for each segment delimited by `splitter`.
    ///
    /// Each `segment` is a `StringView` of the content NOT including the
    /// `splitter` itself. A trailing `splitter` does not produce an extra
    /// empty final segment, matching `std::getline` / `str.splitlines()`
    /// semantics: `"a,b,".forSplits(",", ...)` yields exactly `"a"` and
    /// `"b"`. A leading or embedded back-to-back `splitter` does produce
    /// empty segments. Multi-character splitters use the standard
    /// non-overlapping convention. `splitter` must be non-empty.
    ///
    /// Definitions live in `StringViewSplits.hpp`; include that header to
    /// call these methods.
    ///
    ////////////////////////////////////////////////////////////
    template <typename F>
    constexpr void forSplits(StringView splitter, F&& f) const;


    ////////////////////////////////////////////////////////////
    /// \brief Invoke `f(segment)` for each segment delimited by a single character.
    ///
    /// Same semantics as the `StringView`-splitter overload, but uses the
    /// faster single-character `find(char)` path internally.
    ///
    /// Definitions live in `StringViewSplits.hpp`; include that header to
    /// call these methods.
    ///
    ////////////////////////////////////////////////////////////
    template <typename F>
    constexpr void forSplits(char splitter, F&& f) const;


    ////////////////////////////////////////////////////////////
    /// \brief Invoke `f(line)` for each line in the view, splitting on `'\n'`.
    ///
    /// Convenience wrapper for `forSplits('\n', ...)`. Each `line` excludes
    /// the terminating `'\n'`; a trailing newline does not produce an extra
    /// empty final line. Carriage returns in CRLF endings remain part of
    /// the line view.
    ///
    /// Definitions live in `StringViewSplits.hpp`; include that header to
    /// call these methods.
    ///
    ////////////////////////////////////////////////////////////
    template <typename F>
    constexpr void forLines(F&& f) const;


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] constexpr const char* begin() const noexcept
    {
        return theData;
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] constexpr const char* cbegin() const noexcept
    {
        return theData;
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] constexpr const char* end() const noexcept
    {
        return theData + theSize;
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] constexpr const char* cend() const noexcept
    {
        return theData + theSize;
    }


    ////////////////////////////////////////////////////////////
    template <typename T>
    [[nodiscard, gnu::always_inline]] constexpr T to() const
    {
        return T{theData, theSize};
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] constexpr const char& operator[](const SizeT i) const noexcept
    {
        ZA_ASSERT(theData != nullptr);
        ZA_ASSERT(i < theSize);

        return theData[i];
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] friend inline constexpr bool operator==(const StringView& lhs,
                                                                                         const StringView& rhs) noexcept
    {
        if (lhs.theSize != rhs.theSize)
            return false;

        if (lhs.theSize == 0u)
            return true;

        return constexprMemCmp(lhs.theData, rhs.theData, lhs.theSize) == 0;
    }


    ////////////////////////////////////////////////////////////
    /// \brief Lexicographic byte-wise comparison of two views.
    ///
    /// Treats embedded NUL bytes as ordinary data (the underlying primitive is
    /// `memcmp`, not `strncmp`). The return value's *sign* is what matters and
    /// is what callers like `operator<` consume:
    /// - negative: `*this` is less than `rhs`
    /// - zero    : `*this` equals `rhs`
    /// - positive: `*this` is greater than `rhs`
    ///
    /// The exact magnitude is not normalised to `-1` / `+1` (unlike
    /// `std::string::compare`); it can be any nonzero value returned by the
    /// underlying byte comparison.
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] inline constexpr int compare(const StringView& rhs) const noexcept
    {
        const SizeT minSize = ZA_MIN(theSize, rhs.theSize);

        // Avoid passing potentially-null pointers to `memcmp` for the trivial
        // empty-operand case (well-defined in practice but UB by the C standard).
        const int result = (minSize == 0u) ? 0 : constexprMemCmp(theData, rhs.theData, minSize);

        if (result != 0)
            return result;

        if (theSize < rhs.theSize)
            return -1;

        if (theSize > rhs.theSize)
            return 1;

        return 0;
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] friend inline constexpr bool operator==(const StringView& lhs,
                                                                                         const char* const rhs) noexcept
    {
        return lhs == StringView{rhs};
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] friend inline constexpr bool operator<(const StringView& lhs,
                                                                                        const StringView& rhs) noexcept
    {
        return lhs.compare(rhs) < 0;
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] friend inline constexpr bool operator>(const StringView& lhs,
                                                                                        const StringView& rhs) noexcept
    {
        return rhs < lhs;
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] friend inline constexpr bool operator<=(const StringView& lhs,
                                                                                         const StringView& rhs) noexcept
    {
        return lhs.compare(rhs) <= 0;
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] friend inline constexpr bool operator>=(const StringView& lhs,
                                                                                         const StringView& rhs) noexcept
    {
        return rhs <= lhs;
    }


    ////////////////////////////////////////////////////////////
    template <typename StringLike>
    [[nodiscard, gnu::always_inline]] StringLike toString() const
    {
        return StringLike{theData, theSize};
    }


    ////////////////////////////////////////////////////////////
    [[gnu::always_inline]] friend constexpr void swap(StringView& lhs, StringView& rhs) noexcept
    {
        const char* const tmpData = lhs.theData;
        lhs.theData               = rhs.theData;
        rhs.theData               = tmpData;

        const SizeT tmpSize = lhs.theSize;
        lhs.theSize         = rhs.theSize;
        rhs.theSize         = tmpSize;
    }


    ////////////////////////////////////////////////////////////
    // Member data
    ////////////////////////////////////////////////////////////
    const char* theData{nullptr};
    SizeT       theSize{0u};
};


} // namespace za


namespace za::literals
{
////////////////////////////////////////////////////////////
[[nodiscard]] consteval StringView operator""_sv(const char* const cStr, const SizeT len) noexcept
{
    return StringView{cStr, len};
}

} // namespace za::literals
