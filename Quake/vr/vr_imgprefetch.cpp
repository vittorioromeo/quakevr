// vr_imgprefetch.cpp -- the images the start-up and the first map load decode (the models' authored normal maps, the
// body's and the hands' skins: 40 PNGs of up to 1024 x 1024, 0.14 s on one thread), decoded ahead on worker threads
// while the window and the GL context are made (0.3 s the main thread spends waiting on the driver).
//
// Which ones: the list the last session decoded (`<gamedir>/cache/prefetch.txt`, written at the end of the first map
// load when it changed). At VR_Init (the file system is up) the main thread reads each file as the game's lookup finds
// it (COM_LoadMallocFile) and the workers decode the bytes. Image_LoadImage (image.c) then takes an image decoded
// ahead only if the file it opens holds exactly the same bytes (compared); else it decodes as before. Images not taken
// by the end of the first load are freed there; the workers are joined then, or at shutdown.

#include "vr_engine.hpp"
#include "vr_imgprefetch.hpp"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

extern "C" unsigned char* Image_DecodeMemory(const unsigned char* bytes, int length, int* width, int* height); // image.c

namespace
{

namespace fs = std::filesystem;

struct Item
{
    std::string name; // as looked up: progs/vrbody_00_00_norm.png
    std::vector<unsigned char> bytes;
    unsigned char* pixels = nullptr; // malloc'd by stb_image (the caller frees it, as stbi's own)
    int width = 0, height = 0;
    bool done = false;
    bool taken = false;
};

std::mutex lock;
std::condition_variable doneCv;
std::vector<std::unique_ptr<Item>> items;
std::vector<std::thread> workers;
size_t next = 0;
bool active = false;

std::vector<std::string> manifest; // the list read at the start
std::vector<std::string> decoded;  // this window's decodes (found png/tga/jpg files, over a millisecond)
bool windowOpen = false;

fs::path toPath(const std::string& utf8)
{
    return fs::path(std::u8string(utf8.begin(), utf8.end()));
}

std::string manifestPath()
{
    return std::string{com_gamedir} + "/cache/prefetch.txt";
}

void work()
{
    for(;;)
    {
        Item* it;
        {
            std::lock_guard<std::mutex> g(lock);
            if(next >= items.size())
            {
                return;
            }
            it = items[next++].get();
        }
        int w = 0, h = 0;
        unsigned char* px = Image_DecodeMemory(it->bytes.data(), static_cast<int>(it->bytes.size()), &w, &h);
        {
            std::lock_guard<std::mutex> g(lock);
            it->pixels = px;
            it->width = w;
            it->height = h;
            it->done = true;
        }
        doneCv.notify_all();
    }
}

void joinWorkers()
{
    for(std::thread& t : workers)
    {
        t.join();
    }
    workers.clear();
}

void freeItems()
{
    std::lock_guard<std::mutex> g(lock);
    for(auto& it : items)
    {
        if(!it->taken && it->pixels)
        {
            free(it->pixels);
        }
    }
    items.clear();
    next = 0;
}

} // namespace

namespace qvr::imgprefetch
{

// VR_Init: the last session's list read, its files read, their decoding started.
void start()
{
    windowOpen = true;
    std::ifstream in(toPath(manifestPath()));
    std::string line;
    while(std::getline(in, line))
    {
        if(!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }
        if(!line.empty())
        {
            manifest.push_back(line);
        }
    }
    for(const std::string& name : manifest)
    {
        byte* data = COM_LoadMallocFile(name.c_str(), nullptr);
        if(!data)
        {
            continue;
        }
        auto it = std::make_unique<Item>();
        it->name = name;
        it->bytes.assign(data, data + com_filesize);
        free(data);
        items.push_back(std::move(it));
    }
    if(items.empty())
    {
        return;
    }
    active = true;
    const int threads = std::clamp(static_cast<int>(std::thread::hardware_concurrency()) - 1, 1, 6);
    for(int i = 0; i < std::min<int>(threads, static_cast<int>(items.size())); i++)
    {
        workers.emplace_back(work);
    }
}

// The end of the first map load (or the shutdown): the workers joined, what wasn't taken freed, the list written if
// this session's differs.
void end()
{
    if(!windowOpen)
    {
        return;
    }
    windowOpen = false;
    joinWorkers();
    freeItems();
    active = false;
    if(decoded != manifest && !decoded.empty())
    {
        std::error_code ec;
        fs::create_directories(toPath(std::string{com_gamedir} + "/cache"), ec);
        // (A temporary file of this copy's own: parallel test runs share the folder.)
        const std::string path = manifestPath(),
                          tmp = path + va(".%u.tmp", static_cast<unsigned>(Sys_DoubleTime() * 1e6) & 0xffffffu);
        {
            std::ofstream out(toPath(tmp), std::ios::trunc);
            for(const std::string& name : decoded)
            {
                out << name << "\n";
            }
        }
        fs::rename(toPath(tmp), toPath(path), ec);
    }
    manifest.clear();
    decoded.clear();
}

void shutdown()
{
    joinWorkers();
    freeItems();
    active = false;
    windowOpen = false;
}

} // namespace qvr::imgprefetch

// Image_LoadImage: `name` (with its extension) opened as `f`, `length` bytes: the image decoded ahead if its bytes are
// the file's (malloc'd; *width, *height set), else NULL with `f` where it was.
extern "C" unsigned char* VR_ImagePrefetchTake(const char* name, FILE* f, int length, int* width, int* height)
{
    if(!active)
    {
        return nullptr;
    }
    Item* it = nullptr;
    {
        std::unique_lock<std::mutex> g(lock);
        for(auto& i : items)
        {
            if(!i->taken && i->name == name)
            {
                it = i.get();
                break;
            }
        }
        if(!it)
        {
            return nullptr;
        }
        doneCv.wait(g, [it] { return it->done; });
        it->taken = true; // (freed by the caller if taken, below if not)
    }
    const long pos = ftell(f);
    std::vector<unsigned char> bytes(static_cast<size_t>(std::max(length, 0)));
    const bool same = length == static_cast<int>(it->bytes.size()) && it->pixels &&
                      fread(bytes.data(), 1, bytes.size(), f) == bytes.size() && bytes == it->bytes;
    fseek(f, pos, SEEK_SET);
    if(!same)
    {
        if(it->pixels)
        {
            free(it->pixels);
            it->pixels = nullptr;
        }
        return nullptr;
    }
    unsigned char* px = it->pixels;
    it->pixels = nullptr;
    if(std::find(decoded.begin(), decoded.end(), it->name) == decoded.end())
    {
        decoded.push_back(it->name); // (kept on the list: taken ahead, it took no time here)
    }
    *width = it->width;
    *height = it->height;
    return px;
}

// Image_LoadImage: a png, tga or jpg found and decoded (ahead or not) in this window, for the next session's list.
extern "C" void VR_ImagePrefetchNote(const char* name, double seconds)
{
    if(windowOpen && seconds > 0.001 && std::find(decoded.begin(), decoded.end(), name) == decoded.end())
    {
        decoded.push_back(name);
    }
}

extern "C" void VR_ImagePrefetchEnd()
{
    qvr::imgprefetch::end();
}
