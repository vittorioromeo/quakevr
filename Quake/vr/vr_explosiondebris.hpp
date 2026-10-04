// Physical, harmless incandescent chunks. Client model objects like spent casings: no server edicts or damage callbacks.
#pragma once
#include <glm/glm.hpp>

namespace qvr::explosiondebris
{
void spawn(const glm::vec3& origin);
void frame();
void prepare();
void clear();
void registerCommands();
int liveCount();
}
