// Additional rising tongues of fire over the game's low-poly flame models.
#pragma once
#include <glm/glm.hpp>
namespace qvr::fireparticles
{
void frame();
void emitTorch(int entity, const glm::vec3& at, float scale);
void clear();
void registerCommands();
}
