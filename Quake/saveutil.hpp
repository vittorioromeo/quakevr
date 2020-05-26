#pragma once

#include <ctime>

inline constexpr std::size_t MAX_SAVEGAMES = 20;
inline constexpr std::size_t MAX_AUTOSAVES = 12;

namespace quake::saveutil
{

void doAutosave();

[[nodiscard]] std::time_t& lastAutosaveTime() noexcept;

[[nodiscard]] int timeDiffInSeconds(
    const std::time_t& begin, const std::time_t& end) noexcept;

void scanSaves();

[[nodiscard]] const char* nthSaveFilename(const std::size_t i) noexcept;
[[nodiscard]] const char* nthAutosaveFilename(const std::size_t i) noexcept;

[[nodiscard]] bool isNthSaveLoadable(const std::size_t i) noexcept;
[[nodiscard]] bool isNthAutosaveLoadable(const std::size_t i) noexcept;

[[nodiscard]] const std::time_t& nthAutosaveTimestamp(
    const std::size_t i) noexcept;

} // namespace quake::saveutil
