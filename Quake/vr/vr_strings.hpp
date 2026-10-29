#pragma once

// vr_strings.hpp -- the small string helpers the VR files each used to keep a copy of (prefix/suffix tests on C
// strings, lower case, trimming, appending a printf). Implemented in vr_strings.cpp.

#include "Zancle/Base/SizeT.hpp"
#include "Zancle/String/String.hpp"
#include "Zancle/String/StringView.hpp"

#include <string.h>

namespace qvr::strings
{

// s begins with prefix (strncmp over the prefix's length).
[[nodiscard]] inline bool startsWith(const char* s, const char* prefix)
{
    return !strncmp(s, prefix, strlen(prefix));
}

// s ends with suffix.
[[nodiscard]] inline bool endsWith(const char* s, const char* suffix)
{
    const za::SizeT n = strlen(s);
    const za::SizeT m = strlen(suffix);
    return n >= m && !strcmp(s + n - m, suffix);
}

// A copy with every byte lower-cased (tolower of the unsigned char: ASCII in the C locale).
[[nodiscard]] za::String lower(za::StringView s);
[[nodiscard]] za::String lower(const char* s);

// A copy without the leading and trailing space, tab, CR and LF ("" for an all-blank string).
[[nodiscard]] za::String trim(za::StringView s);

// out += the formatted text, whole however long it is (printf format; the engine's q_vsnprintf underneath).
void appendf(za::String& out, const char* fmt, ...) __attribute__((format(printf, 2, 3)));

} // namespace qvr::strings
