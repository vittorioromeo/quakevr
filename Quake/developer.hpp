#pragma once

#include "cvar.hpp"

extern cvar_t developer;

namespace quake::vr
{

[[nodiscard]] int developerMode() noexcept;

}
