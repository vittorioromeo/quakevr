// vr_strings.cpp -- the VR code's shared string helpers (vr_strings.hpp).

#include "vr_strings.hpp"

#include "vr_engine.hpp"

#include "Zancle/Container/Vector.hpp"

#include <ctype.h>
#include <stdarg.h>

namespace qvr::strings
{

za::String lower(const za::StringView s)
{
    za::String out{s};
    for(char& c : out)
    {
        c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    }
    return out;
}

za::String lower(const char* s)
{
    return lower(za::StringView{s});
}

za::String trim(const za::StringView s)
{
    const za::SizeT a = s.findFirstNotOf(" \t\r\n");
    if(a == za::StringView::nPos)
    {
        return za::String{};
    }
    return za::String{s.substrByPosLen(a, s.findLastNotOf(" \t\r\n") - a + 1)};
}

void appendf(za::String& out, const char* fmt, ...)
{
    char    buf[512];
    va_list args;
    va_start(args, fmt);
    va_list again;
    va_copy(again, args);
    const int n = q_vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    if(n >= 0 && static_cast<size_t>(n) < sizeof(buf))
    {
        out += buf;
    }
    else
    {
        // Longer than the stack buffer: format again into one of the exact size (the old copies cut the text at 511).
        za::Vector<char> big;
        big.resize(static_cast<za::SizeT>(n) + 1);
        q_vsnprintf(big.data(), big.size(), fmt, again);
        out += big.data();
    }
    va_end(again);
}

} // namespace qvr::strings
