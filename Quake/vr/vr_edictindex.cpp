#include "vr_alloccount.h"
// vr_edictindex.cpp -- the server's edicts indexed by classname and by the bits of a few flag fields, kept exact as
// they change, so that find() on .classname and findflags() on those fields step straight to the next match instead of
// walking every edict (an edict is several KB: a walk of 1800 costs ~10 us, and secret2's fight made ~50 a frame).
// docs/vr-port/PERF_DECISIONS.md, 9; ROUND21.md, "QuakeC's scans through an index".
//
// Exact, by construction: an edict's entry is read again from the edict after every change that can move it:
// - QuakeC's writes: every field store is OP_ADDRESS then OP_STOREP. OP_ADDRESS of a watched field records the address
//   (VR_EdictIndex_Address); the STOREP into it marks the edict (VR_EdictIndex_Stored), after the write. A recorded
//   address never stored into is marked when the outermost QuakeC call returns (VR_EdictIndex_TopLevelDone).
// - The engine's: an edict freed or taken (ED_AddToFreeList, ED_RemoveFromFreeList), cleared (ED_ClearEdict, ED_Alloc),
//   parsed (ED_ParseEdict), a client's fields cleared (host_cmd.c), Box3D's classnames: VR_EdictIndex_Touch. The
//   engine writes .flags itself only for the ground, water-jump, god and notarget bits: those bits are not indexed.
// - Text: a classname whose string is the progs' own or a string made once (PR_AllocString: the map's, a save's) is
//   listed under its text; any other (a temp string, a zoned one, an engine pointer) can change under the same
//   string_t without a write, so its edict is "volatile": its text is compared at the query, as find() did.
// - A load, a map, the progs: everything is read again at the next query (VR_EdictIndex_Reset).
// vr_edictindex_verify 1 walks as well on every query and counts any difference (vr_edictindex_stats).

#include "vr_edictindex.hpp"
#include "vr_engine.hpp"
#include "vr_cvars.hpp"
#include "vr_mem.hpp"

#include "Zancle/Container/AnkerlUnorderedDense.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/String/String.hpp"
#include "Zancle/String/StringView.hpp"

#include <string.h>

using namespace qvr;

namespace
{

constexpr int numBits = 24; // a flag field's bits indexed (findflags() asking for any other walks)

struct WatchedFloat
{
    const char* name;
    int mask; // the bits indexed: those only QuakeC writes
};

// The float fields findflags() steps through by the index. .flags: not the bits the engine sets in C (sv_phys.c,
// sv_move.c, sv_user.c, host_cmd.c's god and notarget, Quake VR's ground flag).
constexpr int engineFlagBits = FL_GODMODE | FL_NOTARGET | FL_ONGROUND | FL_PARTIALGROUND | FL_WATERJUMP | FL_JUMPRELEASED;
constexpr int allBits = (1 << numBits) - 1;
constexpr WatchedFloat watchedFloats[] = {
    {"flags", allBits & ~engineFlagBits}, // FL_MONSTER, FL_CLIENT, FL_ITEM loops (liquids, wounds, stealth, shock)
    {"wt_state", allBits},                // the stealth AI's look about: lit wall torches
    {"stl_notice", allBits},              // ... and the bodies it can notice
    {"vr_letgo_fall", allBits},           // VR_Throw_SelfFrame
    {"vr_throw_self", allBits},           // ...
    {"MG_registered", allBits},           // MG_WorldFrame
};
constexpr int maxFloats = static_cast<int>(sizeof(watchedFloats) / sizeof(watchedFloats[0]));
constexpr int classField = static_cast<int>(offsetof(entvars_t, classname) / 4);
constexpr unsigned char watchClassname = 1; // fieldwatch's value for .classname; 2 + i for watchedFloats[i]
constexpr unsigned char allFields = 0xff;   // an edict's pending mask: every watched field

// A classname's text looked up without making a za::String of it (a transparent hash).
struct TextHash
{
    using is_transparent = void;
    [[nodiscard]] za::SizeT operator()(za::StringView s) const { return ankerl::unordered_dense::hash<za::StringView>{}(s); }
};
struct TextEqual
{
    using is_transparent = void;
    [[nodiscard]] bool operator()(za::StringView a, za::StringView b) const { return a == b; }
};

// The index (the server's edicts; the main thread). Released at a map change (rebuilt at the next query).
struct EdictIndex
{
    bool valid{false};
    int words{0};      // a bitset's 64-bit words (max_edicts)
    int maxEdicts{0};
    za::Vector<uint64_t> floatBits;   // [numFloats][numBits][words]: the edicts with that bit set (in use)
    za::Vector<int> floatValue;       // [numFloats][maxEdicts]: the masked bits each edict is listed under
    za::Vector<uint64_t> classBits;   // [buckets][words]: the edicts in use whose classname is that text (stable)
    za::Vector<int> classBucket;      // [maxEdicts]: the bucket each edict is listed under, -1 none
    za::Vector<uint64_t> volatileBits; // [words]: edicts in use whose classname's text is read at the query
    ankerl::unordered_dense::map<za::String, int, TextHash, TextEqual> bucketOfText;
    ankerl::unordered_dense::map<int, int> bucketOfString; // a stable string_t's bucket
    za::Vector<unsigned char> pendingMask; // [maxEdicts]: the fields to read again (0: not pending)
    za::Vector<int> pending;               // the edicts with a pending mask, in order of their first change
    auto members()
    {
        return mem::list(valid, words, maxEdicts, floatBits, floatValue, classBits, classBucket, volatileBits, bucketOfText, bucketOfString, pendingMask, pending);
    }
};
mem::Cache<EdictIndex> edictIndex{"edict index", mem::MapChange};

// What the interpreter and the string table feed (the server VM; the main thread). Not released at a map change:
// sv.qcvm.fieldwatch points into fieldwatch until the progs are cleared (PR_ClearProgs) or loaded again.
struct Recorded
{
    int ofs;            // OP_ADDRESS's result: the field's byte offset from the edicts' start
    unsigned char mask; // the watched field's bit in a pending mask
};
struct Watch
{
    za::Vector<unsigned char> fieldwatch; // [entityfields] 0, watchClassname or 2 + i
    za::Vector<unsigned char> stableSlot; // [known strings] the slot's text is never changed (PR_AllocString)
    za::Vector<Recorded> recorded;        // OP_ADDRESS of a watched field whose STOREP hasn't come
    int numFloats{0};                     // watchedFloats the progs have (as floats)
    int floatField[maxFloats]{};          // [numFloats] the field's offset
    int floatMask[maxFloats]{};           // [numFloats] its bits indexed
    int64_t queries{0}, walks{0}, rebuilds{0}, touches{0}, mismatches{0};
};
Watch watch;

[[nodiscard]] edict_t* edictAt(int e)
{
    return reinterpret_cast<edict_t*>(reinterpret_cast<byte*>(sv.qcvm.edicts) + static_cast<size_t>(e) * sv.qcvm.edict_size);
}

[[nodiscard]] float fieldFloat(const edict_t* ed, int field)
{
    return reinterpret_cast<const float*>(&ed->v)[field];
}

[[nodiscard]] int fieldInt(const edict_t* ed, int field)
{
    return reinterpret_cast<const int*>(&ed->v)[field];
}

// The first set bit in [from, end), or end.
[[nodiscard]] int nextBit(const uint64_t* bits, int from, int end)
{
    if(from >= end)
    {
        return end;
    }
    int w = from >> 6;
    uint64_t word = bits[w] & (~uint64_t{0} << (from & 63));
    const int lastWord = (end - 1) >> 6;
    while(true)
    {
        if(word)
        {
            const int i = (w << 6) + __builtin_ctzll(word);
            return i < end ? i : end;
        }
        if(++w > lastWord)
        {
            return end;
        }
        word = bits[w];
    }
}

void setBit(uint64_t* bits, int e, bool on)
{
    const uint64_t m = uint64_t{1} << (e & 63);
    bits[e >> 6] = on ? (bits[e >> 6] | m) : (bits[e >> 6] & ~m);
}

// A string_t whose text never changes: the progs' own strings, or a known string made once (PR_AllocString).
[[nodiscard]] bool stableString(int s)
{
    if(s >= 0)
    {
        return s < sv.qcvm.stringssize;
    }
    const int slot = -1 - s;
    return slot < sv.qcvm.numknownstrings && slot < static_cast<int>(watch.stableSlot.size()) &&
           watch.stableSlot[slot] != 0;
}

[[nodiscard]] int bucketForText(EdictIndex& ix, za::StringView text)
{
    const auto it = ix.bucketOfText.find(text);
    if(it != ix.bucketOfText.end())
    {
        return it->second;
    }
    const int b = static_cast<int>(ix.bucketOfText.size());
    ix.bucketOfText.emplace(za::String{text}, b);
    ix.classBits.resize(ix.classBits.size() + static_cast<za::SizeT>(ix.words), 0);
    return b;
}

void readClassname(EdictIndex& ix, int e, const edict_t* ed)
{
    int bucket = -1;
    bool isVolatile = false;
    if(!ed->free)
    {
        const int s = fieldInt(ed, classField);
        if(stableString(s))
        {
            const auto it = ix.bucketOfString.find(s);
            if(it != ix.bucketOfString.end())
            {
                bucket = it->second;
            }
            else
            {
                bucket = bucketForText(ix, za::StringView{PR_GetString(s)});
                ix.bucketOfString.emplace(s, bucket);
            }
        }
        else
        {
            isVolatile = true;
        }
    }
    const int old = ix.classBucket[e];
    if(old != bucket)
    {
        if(old >= 0)
        {
            setBit(ix.classBits.data() + static_cast<za::SizeT>(old) * ix.words, e, false);
        }
        if(bucket >= 0)
        {
            setBit(ix.classBits.data() + static_cast<za::SizeT>(bucket) * ix.words, e, true);
        }
        ix.classBucket[e] = bucket;
    }
    setBit(ix.volatileBits.data(), e, isVolatile);
}

void readFloat(EdictIndex& ix, int i, int e, const edict_t* ed)
{
    const int now = ed->free ? 0 : (static_cast<int>(fieldFloat(ed, watch.floatField[i])) & watch.floatMask[i]);
    int& listed = ix.floatValue[static_cast<za::SizeT>(i) * ix.maxEdicts + e];
    int changed = listed ^ now;
    while(changed)
    {
        const int bit = __builtin_ctz(static_cast<unsigned>(changed));
        changed &= changed - 1;
        setBit(ix.floatBits.data() + (static_cast<za::SizeT>(i) * numBits + bit) * ix.words, e, (now >> bit) & 1);
    }
    listed = now;
}

void readEdict(EdictIndex& ix, int e, unsigned char mask)
{
    const edict_t* ed = edictAt(e);
    if(mask & watchClassname)
    {
        readClassname(ix, e, ed);
    }
    for(int i = 0; i < watch.numFloats; i++)
    {
        if(mask & (2u << i))
        {
            readFloat(ix, i, e, ed);
        }
    }
}

// Everything read again from the edicts (the first query after a reset).
void rebuild(EdictIndex& ix)
{
    watch.rebuilds++;
    ix.maxEdicts = sv.qcvm.max_edicts;
    ix.words = (ix.maxEdicts + 63) / 64;
    ix.floatBits.assign(static_cast<za::SizeT>(watch.numFloats) * numBits * ix.words, 0);
    ix.floatValue.assign(static_cast<za::SizeT>(watch.numFloats) * ix.maxEdicts, 0);
    ix.bucketOfText.clear();
    ix.bucketOfString.clear();
    ix.classBits.clear();
    ix.classBucket.assign(static_cast<za::SizeT>(ix.maxEdicts), -1);
    ix.volatileBits.assign(static_cast<za::SizeT>(ix.words), 0);
    ix.pendingMask.assign(static_cast<za::SizeT>(ix.maxEdicts), 0);
    ix.pending.clear();
    for(int e = 1; e < sv.qcvm.num_edicts; e++)
    {
        readEdict(ix, e, allFields);
    }
    ix.valid = true;
}

void flushPending(EdictIndex& ix)
{
    for(const int e : ix.pending)
    {
        readEdict(ix, e, ix.pendingMask[e]);
        ix.pendingMask[e] = 0;
    }
    ix.pending.clear();
}

// The index, up to date, for a query on the server VM; nullptr when the index is off (the caller walks).
[[nodiscard]] EdictIndex* ready()
{
    EdictIndex& ix = edictIndex;
    if(!vr_edictindex.value || qcvm != &sv.qcvm || !sv.qcvm.edicts || watch.fieldwatch.empty())
    {
        ix.valid = false; // (off: changes aren't followed; on again, it starts afresh)
        return nullptr;
    }
    watch.queries++;
    if(!ix.valid || ix.maxEdicts != sv.qcvm.max_edicts)
    {
        rebuild(ix);
    }
    else
    {
        flushPending(ix);
    }
    return &ix;
}

void markPending(int e, unsigned char mask)
{
    EdictIndex& ix = edictIndex;
    if(!ix.valid || e <= 0 || e >= ix.maxEdicts)
    {
        return; // (not built: it will be read whole)
    }
    watch.touches++;
    if(!ix.pendingMask[e])
    {
        ix.pending.pushBack(e);
    }
    ix.pendingMask[e] |= mask;
}

[[nodiscard]] int edictOfOffset(int ofs)
{
    return ofs / sv.qcvm.edict_size;
}

// find()'s walk (PF_Find), from start on: the next edict in use whose field's text is s.
[[nodiscard]] int walkFind(int start, int field, const char* s)
{
    for(int e = start + 1; e < sv.qcvm.num_edicts; e++)
    {
        const edict_t* ed = edictAt(e);
        if(ed->free)
        {
            continue;
        }
        const char* t = PR_GetString(fieldInt(ed, field));
        if(t && !strcmp(t, s))
        {
            return e;
        }
    }
    return 0;
}

// findflags()'s walk (PF_findflags).
[[nodiscard]] int walkFindFlags(int start, int field, int flags)
{
    for(int e = start + 1; e < sv.qcvm.num_edicts; e++)
    {
        const edict_t* ed = edictAt(e);
        if(!ed->free && (static_cast<int>(fieldFloat(ed, field)) & flags))
        {
            return e;
        }
    }
    return 0;
}

void checkResult(const char* what, int start, int found, int walked)
{
    if(found != walked)
    {
        watch.mismatches++;
        if(watch.mismatches <= 20)
        {
            Con_Printf("vr_edictindex ERROR: %s from %d: index %d, walk %d\n", what, start, found, walked);
        }
    }
}

} // namespace

void VR_EdictIndex_ProgsLoaded()
{
    // fieldwatch for the server's progs (the qcvm current: PR_LoadProgs's).
    watch.fieldwatch.assign(static_cast<za::SizeT>(qcvm->progs->entityfields), 0);
    watch.recorded.clear();
    watch.fieldwatch[classField] = watchClassname;
    int n = 0;
    for(const WatchedFloat& w : watchedFloats)
    {
        // (a field the progs lack, or of another type, isn't watched; its findflags() walks)
        const int ofs = ED_FindFieldOffset(w.name);
        if(ofs < 0 || ofs >= qcvm->progs->entityfields || watch.fieldwatch[ofs] != 0)
        {
            continue;
        }
        const ddef_t* def = nullptr;
        for(int i = 0; i < qcvm->progs->numfielddefs; i++)
        {
            if(qcvm->fielddefs[i].ofs == ofs && !strcmp(PR_GetString(qcvm->fielddefs[i].s_name), w.name))
            {
                def = &qcvm->fielddefs[i];
                break;
            }
        }
        if(!def || (def->type & ~DEF_SAVEGLOBAL) != ev_float)
        {
            continue;
        }
        watch.fieldwatch[ofs] = static_cast<unsigned char>(2 + n);
        watch.floatField[n] = ofs;
        watch.floatMask[n] = w.mask;
        n++;
    }
    watch.numFloats = n;
    qcvm->fieldwatch = watch.fieldwatch.data();
    qcvm->fieldwatch_n = static_cast<int>(watch.fieldwatch.size());
    qcvm->watchpending = 0;
    edictIndex.valid = false;
}

extern "C" void VR_EdictIndex_Reset()
{
    edictIndex.valid = false;
}

extern "C" void VR_EdictIndex_Touch(edict_t* ed)
{
    if(!edictIndex.valid || !sv.qcvm.edicts)
    {
        return;
    }
    const byte* p = reinterpret_cast<const byte*>(ed);
    const byte* base = reinterpret_cast<const byte*>(sv.qcvm.edicts);
    if(p < base || p >= base + static_cast<size_t>(sv.qcvm.max_edicts) * sv.qcvm.edict_size)
    {
        return; // (the client VM's)
    }
    markPending(static_cast<int>((p - base) / sv.qcvm.edict_size), allFields);
}

extern "C" void VR_EdictIndex_StringSlot(int slot, int stable)
{
    if(slot < 0)
    {
        return;
    }
    if(slot >= static_cast<int>(watch.stableSlot.size()))
    {
        if(!stable)
        {
            return;
        }
        watch.stableSlot.resize(static_cast<za::SizeT>(slot) + 256, 0);
    }
    if(watch.stableSlot[slot] && !stable)
    {
        edictIndex.valid = false; // (a string made once given back: its edicts' text may change; never seen)
    }
    watch.stableSlot[slot] = static_cast<unsigned char>(stable ? 1 : 0);
}

extern "C" void VR_EdictIndex_Address(int ofs, int watched)
{
    watch.recorded.pushBack(Recorded{ofs, static_cast<unsigned char>(watched == watchClassname ? watchClassname : 2u << (watched - 2))});
    sv.qcvm.watchpending = static_cast<int>(watch.recorded.size());
}

extern "C" void VR_EdictIndex_Stored(int ofs)
{
    for(za::SizeT i = watch.recorded.size(); i-- > 0;)
    {
        if(watch.recorded[i].ofs == ofs)
        {
            markPending(edictOfOffset(ofs), watch.recorded[i].mask);
            watch.recorded.erase(watch.recorded.begin() + static_cast<za::PtrDiffT>(i));
            break;
        }
    }
    sv.qcvm.watchpending = static_cast<int>(watch.recorded.size());
}

extern "C" void VR_EdictIndex_TopLevelDone()
{
    for(const Recorded& r : watch.recorded)
    {
        markPending(edictOfOffset(r.ofs), r.mask);
    }
    watch.recorded.clear();
    sv.qcvm.watchpending = 0;
}

extern "C" int VR_EdictIndex_Find(int start, int field, const char* s)
{
    if(field != classField)
    {
        return -1;
    }
    EdictIndex* ix = ready();
    if(!ix)
    {
        return -1;
    }
    const int end = sv.qcvm.num_edicts;
    const auto it = ix->bucketOfText.find(za::StringView{s});
    const uint64_t* bucket = it != ix->bucketOfText.end() ? ix->classBits.data() + static_cast<za::SizeT>(it->second) * ix->words : nullptr;
    int found = 0;
    for(int e = start + 1; e < end;)
    {
        const int b = bucket ? nextBit(bucket, e, end) : end;
        const int v = nextBit(ix->volatileBits.data(), e, b);
        if(v < b)
        {
            // (a volatile classname: find()'s own test)
            const char* t = PR_GetString(fieldInt(edictAt(v), field));
            if(t && !strcmp(t, s))
            {
                found = v;
                break;
            }
            e = v + 1;
            continue;
        }
        found = b < end ? b : 0;
        break;
    }
    if(vr_edictindex_verify.value)
    {
        watch.walks++;
        const int walked = walkFind(start, field, s);
        checkResult(va("find \"%s\"", s), start, found, walked);
        return walked;
    }
    return found;
}

extern "C" int VR_EdictIndex_FindFlags(int start, int field, int flags)
{
    if(field < 0 || field >= static_cast<int>(watch.fieldwatch.size()) || watch.fieldwatch[field] < 2 || flags <= 0)
    {
        return -1;
    }
    EdictIndex* ix = ready();
    if(!ix)
    {
        return -1;
    }
    const int i = watch.fieldwatch[field] - 2;
    if(i >= watch.numFloats || (flags & ~watch.floatMask[i]))
    {
        return -1; // (an engine-written bit: walked)
    }
    const int end = sv.qcvm.num_edicts;
    int best = end;
    for(int bits = flags; bits; bits &= bits - 1)
    {
        const int bit = __builtin_ctz(static_cast<unsigned>(bits));
        best = nextBit(ix->floatBits.data() + (static_cast<za::SizeT>(i) * numBits + bit) * ix->words, start + 1, best);
    }
    const int found = best < end ? best : 0;
    if(vr_edictindex_verify.value)
    {
        watch.walks++;
        const int walked = walkFindFlags(start, field, flags);
        checkResult(va("findflags %d %d", field, flags), start, found, walked);
        return walked;
    }
    return found;
}

namespace qvr::edictindex
{

namespace
{

// vr_edictindex_stats: the index's counts since the last (and resets them).
void stats_f()
{
    Con_Printf("vr_edictindex: %s, %lld queries, %lld rebuilds, %lld edicts read again, %lld verified, %lld mismatches\n",
        vr_edictindex.value ? "on" : "off", static_cast<long long>(watch.queries), static_cast<long long>(watch.rebuilds),
        static_cast<long long>(watch.touches), static_cast<long long>(watch.walks), static_cast<long long>(watch.mismatches));
    watch.queries = watch.rebuilds = watch.touches = watch.walks = watch.mismatches = 0;
}

} // namespace

void registerCommands()
{
    Cmd_AddCommand("vr_edictindex_stats", stats_f);
}

} // namespace qvr::edictindex
