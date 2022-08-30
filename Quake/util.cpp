#include "util.hpp"

#include <GL/glew.h>

namespace quake::util
{

[[nodiscard]] int getMaxMSAALevel() noexcept
{
    int res;
    glGetIntegerv(GL_MAX_SAMPLES, &res);
    return res;
}

} // namespace quake::util
