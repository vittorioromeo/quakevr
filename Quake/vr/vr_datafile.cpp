// vr_datafile.cpp -- see vr_slottable.hpp (readDataFile: the data tables' files, quakevr/data/*.txt).

#include "vr_slottable.hpp"

#include "Zancle/Container/AnkerlUnorderedDense.hpp"

#include <stdlib.h>
#include <string.h>

namespace qvr
{

bool readDataFile(const DataFileSpec& spec, za::Span<za::String> defaults, za::Vector<DataFileValue>& own)
{
    for(za::String& d : defaults)
    {
        d = "0";
    }
    ankerl::unordered_dense::map<za::String, int> keyOf;
    for(za::SizeT key = 0; key < spec.keyNames.size(); key++)
    {
        keyOf[za::String{spec.keyNames[key]}] = static_cast<int>(key);
    }
    byte* file = COM_LoadMallocFile(spec.path, nullptr);
    if(!file)
    {
        return false;
    }
    int problems = 0;
    const char* data = reinterpret_cast<const char*>(file);
    while((data = COM_Parse(data)) != nullptr)
    {
        int slot = -1; // (the defaults block)
        if(!strcmp(com_token, spec.block))
        {
            data = COM_Parse(data);
            if(data && !spec.slotNames.empty())
            {
                for(za::SizeT s = 0; s < spec.slotNames.size(); s++)
                {
                    if(!strcmp(com_token, spec.slotNames[s]))
                    {
                        slot = static_cast<int>(s);
                    }
                }
                if(slot < 0)
                {
                    Con_Printf("\x02%s: \"%s %s\": no such %s (%s)\n", spec.path, spec.block, com_token, spec.block, spec.keysFile);
                    break;
                }
            }
            else
            {
                slot = data ? Q_atoi(com_token) : -1;
            }
            if(!data || slot < 0 || slot >= spec.slots)
            {
                Con_Printf("\x02%s: \"%s %s\": no such slot (0 .. %d)\n", spec.path, spec.block, com_token, spec.slots - 1);
                break;
            }
        }
        else if(strcmp(com_token, "defaults") != 0)
        {
            Con_Printf("\x02%s: \"%s\": expected \"defaults\" or \"%s <slot>\"\n", spec.path, com_token, spec.block);
            break;
        }
        data = COM_Parse(data);
        if(!data || strcmp(com_token, "{") != 0)
        {
            Con_Printf("\x02%s: expected \"{\" after a block's name\n", spec.path);
            break;
        }
        while((data = COM_Parse(data)) != nullptr && strcmp(com_token, "}") != 0)
        {
            za::String name{com_token};
            data = COM_Parse(data);
            if(!data)
            {
                break;
            }
            const auto it = keyOf.find(name);
            if(it == keyOf.end())
            {
                Con_Printf("\x02%s: no key \"%s\" (%s): ignored\n", spec.path, name.cStr(), spec.keysFile);
                problems++;
                continue;
            }
            if(slot < 0)
            {
                defaults[static_cast<za::SizeT>(it->second)] = com_token;
            }
            else
            {
                own.pushBack(DataFileValue{slot, it->second, za::String{com_token}});
            }
        }
    }
    free(file);
    if(problems > 0)
    {
        Con_Printf("\x02%s: %d line%s ignored\n", spec.path, problems, problems == 1 ? "" : "s");
    }
    return true;
}

} // namespace qvr
