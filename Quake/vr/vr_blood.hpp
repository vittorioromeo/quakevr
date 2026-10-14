// Peripheral damage feedback. Uses svc_damage and local STAT_HEALTH; cosmetic only.
#pragma once
namespace qvr::blood
{
void clear();
void registerCommands();
void onDamage(int armor, int blood, const float* from);
[[nodiscard]] bool active();
void bind();
}
