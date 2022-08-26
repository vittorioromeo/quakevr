#pragma once

#ifdef WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include "debugapi.h"

#undef min
#undef max
#endif

#include "stringcat.hpp"

namespace quake::util
{

template <typename... Ts>
void debugPrint([[maybe_unused]] const Ts&... xs)
{
#ifdef WIN32
    OutputDebugStringA(stringCat(xs...).data());
#endif
}

template <typename... Ts>
void debugPrintSeparated([[maybe_unused]] const std::string_view separator,
    [[maybe_unused]] const Ts&... xs)
{
#ifdef WIN32
    OutputDebugStringA(stringCatSeparated(separator, xs...).data());
#endif
}

} // namespace quake::util
