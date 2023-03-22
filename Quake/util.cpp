#include "util.hpp"

#include <GL/glew.h>

namespace quake::util
{

[[nodiscard]] int getMaxMSAALevel() noexcept
{
    static int result = []
    {
        int res;
        glGetIntegerv(GL_MAX_SAMPLES, &res);
        return res;
    }();

    return result;
}

} // namespace quake::util
