#include "vr_alloccount.h"
// vr_imgcache.cpp -- decoded images kept across map loads (vr_image_cache_mb). A map's png, tga and jpg files (a texture
// pack's world textures and their material maps, skies, skins) are decoded again at every load otherwise: Ironwail frees
// the world's textures with the map (e1m1 on QRP: 500 ms of its 800 ms warm load decoding the same files again).
// Image_LoadImage (image.c) asks here once it has opened a file, before decoding it, and gives what it decoded.
//
// The key is the file as opened, not its name: the file on disk (its volume and file index, or device and inode), its
// last write time and size, where the image starts in it (a pak's entry) and its length, and the name looked up. A
// file changed or replaced, another game folder's or pack's file of the same name, another pak: another key (the old
// image of that name is dropped when the new one is kept). The decoding has no settings (stb_image, always RGBA); what
// the textures' settings do comes after, on the caller's copy. The images used least recently go first once the
// cache holds more than vr_image_cache_mb (0: none kept). Main thread only (Image_LoadImage allocates on the hunk).

#include "vr_imgcache.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_files.hpp"
#include "vr_mem.hpp"

#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Base/Macros.hpp"
#include "Zancle/Container/AnkerlUnorderedDense.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/String/String.hpp"
#include "Zancle/String/StringView.hpp"

#include <string.h>

namespace
{

struct Key
{
    za::String name; // as looked up: textures/e1m1/city4_6.tga
    qvr::files::Identity file;
    za::I64 offset{0}; // where the image starts in the file
    za::I64 length{0};
    [[nodiscard]] bool operator==(const Key&) const = default;
};

// The image: its pixels (RGBA, malloc'd by stb_image: freed with VR_HeapFree).
struct Image
{
    Key key;
    unsigned char* pixels{nullptr};
    int width{0};
    int height{0};
    za::U64 used{0}; // the use counter's value at its last use (the least recent goes first)

    Image() = default;
    Image(const Image&) = delete;
    Image& operator=(const Image&) = delete;
    Image(Image&& o) noexcept :
        key(ZA_MOVE(o.key)), pixels(o.pixels), width(o.width), height(o.height), used(o.used)
    {
        o.pixels = nullptr;
    }
    Image& operator=(Image&& o) noexcept
    {
        if(this != &o)
        {
            VR_HeapFree(pixels);
            key = ZA_MOVE(o.key);
            pixels = o.pixels;
            width = o.width;
            height = o.height;
            used = o.used;
            o.pixels = nullptr;
        }
        return *this;
    }
    ~Image() { VR_HeapFree(pixels); }

    [[nodiscard]] za::SizeT bytes() const { return static_cast<za::SizeT>(width) * static_cast<za::SizeT>(height) * 4u; }
};

[[nodiscard]] za::SizeT heldBytes(const Image& i)
{
    return i.bytes() + qvr::mem::heldBytes(i.key.name);
}

struct NameHash
{
    using is_transparent = void;
    [[nodiscard]] za::U64 operator()(za::StringView s) const { return ankerl::unordered_dense::hash<za::StringView>{}(s); }
};
struct NameEqual
{
    using is_transparent = void;
    [[nodiscard]] bool operator()(za::StringView a, za::StringView b) const { return a == b; }
};

struct ImageCache
{
    za::Vector<Image> images; // (an image removed: the last one moved into its place)
    ankerl::unordered_dense::map<za::String, za::SizeT, NameHash, NameEqual> byName; // name -> its index in images
    auto members() { return qvr::mem::list(images, byName); }
};
qvr::mem::Cache<ImageCache> cache{"decoded images", qvr::mem::Never}; // (vr_image_cache_mb; vr_image_cache_info)

za::U64 useCounter = 0;
int hits = 0, misses = 0, kept = 0, dropped = 0; // since the start (vr_image_cache_info)
double hitMs = 0.0;                              // ... the hits' copies and lookups

// The images' pixels, in bytes.
[[nodiscard]] za::SizeT pixelBytes()
{
    za::SizeT n = 0;
    for(const Image& i : cache.images)
    {
        n += i.bytes();
    }
    return n;
}

[[nodiscard]] za::SizeT capBytes()
{
    const float mb = qvr::vr_image_cache_mb.value;
    return mb > 0.f ? static_cast<za::SizeT>(static_cast<double>(mb) * 1048576.0) : 0u;
}

void removeAt(za::SizeT i)
{
    za::Vector<Image>& images = cache.images;
    const auto it = cache.byName.find(za::StringView{images[i].key.name});
    if(it != cache.byName.end())
    {
        cache.byName.erase(it);
    }
    if(i + 1 != images.size())
    {
        images[i] = ZA_MOVE(images.back());
        cache.byName[images[i].key.name] = i;
    }
    images.popBack();
    ++dropped;
}

// Down to `cap` bytes, the least recently used first.
void trim(za::SizeT cap)
{
    za::SizeT held = pixelBytes();
    while(held > cap && !cache.images.empty())
    {
        za::SizeT oldest = 0;
        for(za::SizeT i = 1; i < cache.images.size(); ++i)
        {
            if(cache.images[i].used < cache.images[oldest].used)
            {
                oldest = i;
            }
        }
        held -= cache.images[oldest].bytes();
        removeAt(oldest);
    }
}

[[nodiscard]] bool makeKey(const char* name, FILE* f, int length, Key& out)
{
    if(!f || length <= 0 || !qvr::files::identity(f, out.file))
    {
        return false;
    }
    out.name = name;
    out.offset = static_cast<za::I64>(ftell(f));
    out.length = length;
    return out.offset >= 0;
}

void info_f()
{
    Con_Printf("vr_image_cache: %d images, %.1f MB of %.0f MB (vr_image_cache_mb); since the start %d found (%.1f ms), "
               "%d decoded, %d kept, %d dropped\n",
        static_cast<int>(cache.images.size()), static_cast<double>(pixelBytes()) / 1048576.0,
        static_cast<double>(qvr::vr_image_cache_mb.value), hits, hitMs, misses, kept, dropped);
}

void clear_f()
{
    trim(0);
    Con_Printf("vr_image_cache: emptied\n");
}

} // namespace

namespace qvr::imgcache
{

void init()
{
    Cmd_AddCommand("vr_image_cache_info", info_f);
    Cmd_AddCommand("vr_image_cache_clear", clear_f);
}

} // namespace qvr::imgcache

// Image_LoadImage, a png, tga or jpg file opened at `f` (`length` bytes from where it stands): its pixels decoded before
// (copied into `out`, which `alloc` gives: the caller's hunk), or NULL.
extern "C" unsigned char* VR_ImageCacheFind(const char* name, FILE* f, int length, int* width, int* height,
    unsigned char* (*alloc)(int bytes, const char* what), const char* what)
{
    if(capBytes() == 0)
    {
        trim(0); // (turned off: given back)
        return nullptr;
    }
    const double t0 = Sys_DoubleTime();
    Key key;
    if(!makeKey(name, f, length, key))
    {
        return nullptr;
    }
    const auto it = cache.byName.find(za::StringView{key.name});
    if(it == cache.byName.end() || !(cache.images[it->second].key == key))
    {
        ++misses;
        return nullptr;
    }
    Image& image = cache.images[it->second];
    image.used = ++useCounter;
    unsigned char* out = alloc(static_cast<int>(image.bytes()), what);
    memcpy(out, image.pixels, image.bytes());
    *width = image.width;
    *height = image.height;
    ++hits;
    hitMs += (Sys_DoubleTime() - t0) * 1000.0;
    return out;
}

// ... and what it decoded there (malloc'd RGBA, `width` x `height`): kept (true: the cache frees it) or not (false: the
// caller frees it). `f` stands where it stood at VR_ImageCacheFind.
extern "C" int VR_ImageCachePut(const char* name, FILE* f, int length, unsigned char* pixels, int width, int height)
{
    const za::SizeT cap = capBytes();
    const za::SizeT bytes = static_cast<za::SizeT>(width) * static_cast<za::SizeT>(height) * 4u;
    if(cap == 0 || !pixels || width <= 0 || height <= 0 || bytes > cap / 4u) // (one image at most a quarter of it)
    {
        return 0;
    }
    Image image;
    if(!makeKey(name, f, length, image.key))
    {
        return 0;
    }
    const auto it = cache.byName.find(za::StringView{image.key.name});
    if(it != cache.byName.end())
    {
        removeAt(it->second); // (the same name from another file, or this one changed: the old one goes)
    }
    trim(cap - bytes);
    image.pixels = pixels;
    image.width = width;
    image.height = height;
    image.used = ++useCounter;
    cache.byName[image.key.name] = cache.images.size();
    cache.images.pushBack(ZA_MOVE(image));
    ++kept;
    return 1;
}
