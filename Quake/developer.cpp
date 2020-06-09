#include "developer.hpp"

namespace quake::vr
{

[[nodiscard]] int developerMode() noexcept
{
#ifndef NDEBUG
    return 1;
#endif

    return developer.value;
}

} // namespace quake::vr
