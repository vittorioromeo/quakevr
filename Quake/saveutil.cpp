#include "saveutil.hpp"

#include "quakedef_macros.hpp"
#include "q_stdinc.hpp"
#include "common.hpp"
#include "console.hpp"
#include "vr_cvars.hpp"
#include "host.hpp"

#include <algorithm>
#include <cassert>
#include <iterator>
#include <iostream>

namespace quake::saveutil
{
[[nodiscard]] auto& saveFilenames() noexcept
{
    static char data[MAX_SAVEGAMES][SAVEGAME_COMMENT_LENGTH + 1]{};
    return data;
}

[[nodiscard]] auto& autosaveFilenames() noexcept
{
    static char data[MAX_AUTOSAVES][SAVEGAME_COMMENT_LENGTH + 1]{};
    return data;
}

[[nodiscard]] auto& saveLoadables() noexcept
{
    static int data[MAX_SAVEGAMES]{};
    return data;
}

[[nodiscard]] auto& autosaveLoadables() noexcept
{
    static int data[MAX_AUTOSAVES]{};
    return data;
}

[[nodiscard]] auto& autosaveTimestamps() noexcept
{
    static std::time_t data[MAX_AUTOSAVES]{};
    return data;
}

[[nodiscard]] const char* nthSaveFilename(const std::size_t i) noexcept
{
    assert(i < MAX_SAVEGAMES);
    return saveFilenames()[i];
}
// namespace quake::saveutil
[[nodiscard]] const char* nthAutosaveFilename(const std::size_t i) noexcept
{
    assert(i < MAX_AUTOSAVES);
    return autosaveFilenames()[i];
}

[[nodiscard]] bool isNthSaveLoadable(const std::size_t i) noexcept
{
    assert(i < MAX_SAVEGAMES);
    return saveLoadables()[i];
}

[[nodiscard]] bool isNthAutosaveLoadable(const std::size_t i) noexcept
{
    assert(i < MAX_AUTOSAVES);
    return autosaveLoadables()[i];
}

[[nodiscard]] const std::time_t& nthAutosaveTimestamp(
    const std::size_t i) noexcept
{
    assert(i < MAX_AUTOSAVES);
    return autosaveTimestamps()[i];
}

[[nodiscard]] int timeDiffInSeconds(
    const std::time_t& begin, const std::time_t& end) noexcept
{
    return std::difftime(end, begin);
}

[[nodiscard]] std::time_t& lastAutosaveTime() noexcept
{
    static std::time_t res{std::time(nullptr)};
    return res;
}

void scanSaves()
{
    const auto doScan = [&](auto& filenamesArray, auto& loadableArray,
                            const int max, const char* naming,
                            const char* unusedSlot, std::time_t* timestampArray)
    {
        for(int i = 0; i < max; i++)
        {
            strcpy(filenamesArray[i], unusedSlot);
            loadableArray[i] = false;

            char name[MAX_OSPATH];
            q_snprintf(name, sizeof(name), naming, com_gamedir, i);

            FILE* f = fopen(name, "r");
            if(!f)
            {
                continue;
            }

            if(timestampArray != nullptr)
            {
                std::tm tm{};
                tm.tm_isdst = 0;

                std::fscanf(f, "%d-%d-%d %d:%d:%d\n", &tm.tm_year, &tm.tm_mon,
                    &tm.tm_mday, &tm.tm_hour, &tm.tm_min, &tm.tm_sec);
                tm.tm_year -= 1900;

                timestampArray[i] = std::mktime(&tm);
                if(timestampArray[i] == -1)
                {
                    std::cerr << "Error loading timestamp for save '" << name
                              << "'\n";

                    loadableArray[i] = false;
                    fclose(f);
                    continue;
                }
            }

            int version;
            fscanf(f, "%i\n", &version);
            fscanf(f, "%79s\n", name);
            q_strlcpy(filenamesArray[i], name, SAVEGAME_COMMENT_LENGTH + 1);

            // change _ back to space
            for(int j = 0; j < SAVEGAME_COMMENT_LENGTH; j++)
            {
                if(filenamesArray[i][j] == '_')
                {
                    filenamesArray[i][j] = ' ';
                }
            }

            loadableArray[i] = true;
            fclose(f);
        }
    };

    doScan(                    //
        saveFilenames(),       //
        saveLoadables(),       //
        MAX_SAVEGAMES,         //
        "%s/s%i.sav",          //
        "--- UNUSED SLOT ---", //
        nullptr                //
    );

    doScan(                             //
        autosaveFilenames(),            //
        autosaveLoadables(),            //
        MAX_AUTOSAVES,                  //
        "%s/auto%i.sav",                //
        "--- UNUSED AUTOSAVE SLOT ---", //
        autosaveTimestamps()            //
    );
}

[[nodiscard]] static int getNextAutosaveSlot() noexcept
{
    scanSaves();

    for(std::size_t i = 0; i < MAX_AUTOSAVES; ++i)
    {
        if(!isNthAutosaveLoadable(i))
        {
            return i;
        }
    }

    const auto it = std::min_element(std::begin(autosaveTimestamps()),
        std::end(autosaveTimestamps()),
        [](const std::time_t& a, const std::time_t& b)
        {
            return std::difftime(a, std::time_t(nullptr)) <
                   std::difftime(b, std::time_t(nullptr));
        });

    return it - std::begin(autosaveTimestamps());
}

void doAutosave() noexcept
{
    if(vr_autosave_show_message.value)
    {
        Con_Printf("Creating autosave...\n");
    }

    const int idx = getNextAutosaveSlot();
    const std::time_t now = std::time(nullptr);

    char name[64];
    q_snprintf(name, sizeof(name), "auto%d", idx);

    if(Host_MakeSavegame(name, &now, vr_autosave_show_message.value))
    {
        if(vr_autosave_show_message.value)
        {
            Con_Printf("Successfully created autosave.\n");
        }
    }
    else
    {
        if(vr_autosave_show_message.value)
        {
            Con_Printf("Failed to created autosave.\n");
        }
    }
}

void doAutomaticAutosave() noexcept
{
    const std::time_t now = std::time(nullptr);

    const int secondDiff = quake::saveutil::timeDiffInSeconds(
        quake::saveutil::lastAutosaveTime(), now);

    if(secondDiff > vr_autosave_seconds.value)
    {
        quake::saveutil::doAutosave();
        quake::saveutil::lastAutosaveTime() = now;
    }
}

void doChangelevelAutosave() noexcept
{
    if(!vr_autosave_on_changelevel.value)
    {
        return;
    }

    const std::time_t now = std::time(nullptr);

    quake::saveutil::doAutosave();
    quake::saveutil::lastAutosaveTime() = now;
}

} // namespace quake::saveutil
