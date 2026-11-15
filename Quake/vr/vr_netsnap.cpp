// vr_netsnap.cpp -- snapshots down, acknowledged and delta-coded, and the entities' interpolation buffer (vr_netsnap.hpp;
// MP_PLAN.md, B7b).

#include "vr_netsnap.hpp"
#include "vr_clientprefs.hpp"
#include "vr_netmove.hpp"
#include "vr_mpavatar.hpp"
#include "vr_engine.hpp"
#include "vr_api.h"
#include "vr_cvars.hpp"
#include "vr_protocol.hpp"
#include "vr_testkit.hpp"

#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/MinMax.hpp"

#include <algorithm>
#include <string.h>

extern "C" void CL_ParseUpdate(int bits);  // cl_parse.c: an entity's update (its first byte read)
extern "C" void CL_ParseClientdata(void);  // cl_parse.c: svc_clientdata's body

namespace qvr::netsnap
{

void Snapshot::clear()
{
    seq = 0;
    records.clear();
    bytes.clear();
}

void Snapshot::add(int key, const za::U8* d, int len)
{
    records.emplaceBack(Record{key, static_cast<za::I32>(bytes.size()), len});
    if(len > 0)
    {
        bytes.emplaceBackRange(d, static_cast<za::SizeT>(len));
    }
}

void Snapshot::sortByKey()
{
    std::sort(records.begin(), records.end(), [](const Record& a, const Record& b) { return a.key < b.key; });
}

const za::U8* Snapshot::data(const Record& r) const
{
    return bytes.data() + r.at;
}

namespace
{

constexpr int opGone = 0;
constexpr int opWhole = 1;
constexpr int opDelta = 2;
constexpr int maxDeltaLen = 2048; // a longer record always goes whole
constexpr int headerMost = 2 + 5 * 3; // svc_quakevr, the sub-command and three varints

[[nodiscard]] int varintSize(za::U32 u)
{
    int n = 1;
    for(; u >= 0x80u; u >>= 7)
    {
        n++;
    }
    return n;
}

[[nodiscard]] za::U32 entryHead(int key, int prevKey, int op)
{
    return (static_cast<za::U32>(key - prevKey) << 2) | static_cast<za::U32>(op);
}

// `cur`'s bytes as a delta from `base`'s (the same length): a byte a 8 groups (any changed), a mask byte each changed
// group, then its changed bytes. Its size; written into `buf` unless nullptr.
int bytesDelta(sizebuf_t* buf, const za::U8* base, const za::U8* cur, int len)
{
    const int groups = (len + 7) / 8;
    const int tops = (groups + 7) / 8;
    za::U8 masks[maxDeltaLen / 8]{};
    za::U8 top[maxDeltaLen / 64]{};
    int size = tops;
    for(int i = 0; i < len; i++)
    {
        if(base[i] != cur[i])
        {
            masks[i / 8] = static_cast<za::U8>(masks[i / 8] | (1 << (i % 8)));
            size++;
        }
    }
    for(int g = 0; g < groups; g++)
    {
        if(masks[g])
        {
            top[g / 8] = static_cast<za::U8>(top[g / 8] | (1 << (g % 8)));
            size++;
        }
    }
    if(!buf)
    {
        return size;
    }
    for(int t = 0; t < tops; t++)
    {
        MSG_WriteByte(buf, top[t]);
    }
    for(int g = 0; g < groups; g++)
    {
        if(!masks[g])
        {
            continue;
        }
        MSG_WriteByte(buf, masks[g]);
        for(int k = 0; k < 8; k++)
        {
            if(masks[g] & (1 << k))
            {
                MSG_WriteByte(buf, cur[g * 8 + k]);
            }
        }
    }
    return size;
}

// bytesDelta's, read from net_message (no further than `end`) onto `out` (the base's bytes): false when malformed.
[[nodiscard]] bool readBytesDelta(za::U8* out, int len, int end)
{
    const int groups = (len + 7) / 8;
    const int tops = (groups + 7) / 8;
    za::U8 top[maxDeltaLen / 64]{};
    if(len > maxDeltaLen)
    {
        return false;
    }
    for(int t = 0; t < tops; t++)
    {
        if(msg_readcount >= end)
        {
            return false;
        }
        top[t] = static_cast<za::U8>(MSG_ReadByte());
    }
    for(int g = 0; g < groups; g++)
    {
        if(!(top[g / 8] & (1 << (g % 8))))
        {
            continue;
        }
        if(msg_readcount >= end)
        {
            return false;
        }
        const int mask = MSG_ReadByte();
        for(int k = 0; k < 8; k++)
        {
            if(!(mask & (1 << k)))
            {
                continue;
            }
            const int i = g * 8 + k;
            if(i >= len || msg_readcount >= end)
            {
                return false;
            }
            out[i] = static_cast<za::U8>(MSG_ReadByte());
        }
    }
    return !msg_badread;
}

void addFrom(Snapshot& to, const Snapshot& from, const Record& r)
{
    to.add(r.key, from.data(r), r.len);
}

// ---- B12b: an entity's update by its fields ---------------------------------------------------------------------------
//
// NetQuake's entity update (sv_main.c SV_WriteEntitiesToClient: its bits, its number, then each field its bits name, then
// Quake VR's own, vr_server.cpp VR_WriteEntityUpdate) changes its layout whenever a field comes or goes, and a moving
// thing's fields change every frame: byte by byte, against the base, a grunt's update cost about 10 B a frame. By its
// fields (opFields): [varint the changed fields' mask (fieldOrder: the ones that change most first; then the tail's, the
// bits')][varint the bits xor the base's, if changed][each changed field: zigzag varint its change modulo its size][the
// tail (what follows the last field), if changed: varint (length << 1 | delta), then bytesDelta of the base's or its
// bytes]. A field the base lacks counts from 0. Lossless whatever the bytes mean: the server only sends it when the
// client's decoding of it gives back the update exactly (fieldsVerify), else byte by byte or whole as before.

constexpr int opFields = 3;
constexpr int wireFields = 16; // model, frame, colormap, skin, effects, origin1, angle1, origin2, angle2, origin3, angle3,
                               // alpha, scale, frame2, model2, lerpfinish: their order in the update
constexpr int wireBit[wireFields] = {U_MODEL, U_FRAME, U_COLORMAP, U_SKIN, U_EFFECTS, U_ORIGIN1, U_ANGLE1, U_ORIGIN2,
    U_ANGLE2, U_ORIGIN3, U_ANGLE3, U_ALPHA, U_SCALE, U_FRAME2, U_MODEL2, U_LERPFINISH};
constexpr int fieldOrder[wireFields] = {15, 1, 5, 7, 9, 8, 6, 10, 0, 2, 3, 4, 11, 12, 13, 14}; // mask bit -> field
constexpr za::U32 maskTail = 1u << wireFields, maskBits = 1u << (wireFields + 1);
constexpr int fieldsMost = maxDeltaLen + 64; // an update's bytes at most (a longer one: not by its fields)

struct EntFields
{
    za::U32 bits{0};
    int num{0};
    za::U32 v[wireFields]{}; // each present field's bytes as a little-endian number (absent: 0)
    const za::U8* tail{nullptr};
    int tailLen{0};
};

[[nodiscard]] int fieldSize(int f, unsigned pflags)
{
    const bool coord = f == 5 || f == 7 || f == 9, angle = f == 6 || f == 8 || f == 10;
    if(coord)
    {
        return (pflags & (PRFL_FLOATCOORD | PRFL_INT32COORD)) ? 4 : (pflags & PRFL_24BITCOORD) ? 3 : 2;
    }
    if(angle)
    {
        return (pflags & PRFL_FLOATANGLE) ? 4 : (pflags & PRFL_SHORTANGLE) ? 2 : 1;
    }
    return 1;
}

// An update's bytes into its fields: false when they are not one (too short).
[[nodiscard]] bool parseFields(const za::U8* d, int len, unsigned pflags, EntFields& e)
{
    int at = 0;
    const auto take = [&](int n, za::U32& out) {
        if(at + n > len)
        {
            return false;
        }
        out = 0;
        for(int k = 0; k < n; k++)
        {
            out |= static_cast<za::U32>(d[at + k]) << (8 * k);
        }
        at += n;
        return true;
    };
    za::U32 b = 0;
    if(!take(1, e.bits) || !(e.bits & U_SIGNAL))
    {
        return false;
    }
    if((e.bits & U_MOREBITS) && !(take(1, b) && ((e.bits |= b << 8), true)))
    {
        return false;
    }
    if((e.bits & U_EXTEND1) && !(take(1, b) && ((e.bits |= b << 16), true)))
    {
        return false;
    }
    if((e.bits & U_EXTEND2) && !(take(1, b) && ((e.bits |= b << 24), true)))
    {
        return false;
    }
    za::U32 num = 0;
    if(!take((e.bits & U_LONGENTITY) ? 2 : 1, num))
    {
        return false;
    }
    e.num = static_cast<int>(num);
    for(int f = 0; f < wireFields; f++)
    {
        e.v[f] = 0;
        if((e.bits & static_cast<za::U32>(wireBit[f])) && !take(fieldSize(f, pflags), e.v[f]))
        {
            return false;
        }
    }
    e.tail = d + at;
    e.tailLen = len - at;
    return true;
}

// The fields back into an update's bytes (at most `most`): its length, -1 when it does not fit.
[[nodiscard]] int writeFields(const EntFields& e, unsigned pflags, za::U8* out, int most)
{
    int at = 0;
    const auto put = [&](za::U32 v, int n) {
        for(int k = 0; k < n && at < most; k++)
        {
            out[at++] = static_cast<za::U8>(v >> (8 * k));
        }
    };
    put(e.bits, 1);
    if(e.bits & U_MOREBITS)
    {
        put(e.bits >> 8, 1);
    }
    if(e.bits & U_EXTEND1)
    {
        put(e.bits >> 16, 1);
    }
    if(e.bits & U_EXTEND2)
    {
        put(e.bits >> 24, 1);
    }
    put(static_cast<za::U32>(e.num), (e.bits & U_LONGENTITY) ? 2 : 1);
    for(int f = 0; f < wireFields; f++)
    {
        if(e.bits & static_cast<za::U32>(wireBit[f]))
        {
            put(e.v[f], fieldSize(f, pflags));
        }
    }
    if(at + e.tailLen > most)
    {
        return -1;
    }
    memcpy(out + at, e.tail, static_cast<size_t>(e.tailLen));
    return at + e.tailLen;
}

int putVarint(za::U8* out, za::U32 v)
{
    int n = 0;
    for(; v >= 0x80u; v >>= 7)
    {
        out[n++] = static_cast<za::U8>(v | 0x80u);
    }
    out[n++] = static_cast<za::U8>(v);
    return n;
}

[[nodiscard]] bool getVarint(const za::U8* d, int len, int& at, za::U32& v)
{
    v = 0;
    for(int shift = 0; shift < 35; shift += 7)
    {
        if(at >= len)
        {
            return false;
        }
        const za::U8 b = d[at++];
        v |= static_cast<za::U32>(b & 0x7f) << shift;
        if(!(b & 0x80))
        {
            return true;
        }
    }
    return false;
}

// A field's change modulo its size (`n` bytes), zigzagged: small either way.
[[nodiscard]] za::U32 fieldChange(za::U32 base, za::U32 cur, int n)
{
    const za::U32 mask = n >= 4 ? 0xFFFFFFFFu : (1u << (8 * n)) - 1u;
    const za::U32 d = (cur - base) & mask;
    const za::U32 half = n >= 4 ? 0x80000000u : 1u << (8 * n - 1);
    const za::I64 s = d >= half ? static_cast<za::I64>(d) - static_cast<za::I64>(mask) - 1 : static_cast<za::I64>(d);
    return static_cast<za::U32>(s >= 0 ? s * 2 : -s * 2 - 1);
}

[[nodiscard]] za::U32 fieldApply(za::U32 base, za::U32 zig, int n)
{
    const za::U32 mask = n >= 4 ? 0xFFFFFFFFu : (1u << (8 * n)) - 1u;
    const za::I64 s = (zig & 1u) ? -static_cast<za::I64>(zig >> 1) - 1 : static_cast<za::I64>(zig >> 1);
    return (base + static_cast<za::U32>(s)) & mask;
}

za::U8 fieldsBuf[fieldsMost];   // (an encoding by fields: encode's)
za::U8 fieldsCheck[fieldsMost]; // (... its decoding: fieldsVerify's, decode's)

// `cur` (an entity's update) by its fields against `base`'s into `out` (at most fieldsMost): its size, -1 when not.
[[nodiscard]] int fieldsEncode(const za::U8* base, int baseLen, const za::U8* cur, int curLen, unsigned pflags, za::U8* out)
{
    EntFields b, c;
    if(curLen > maxDeltaLen || !parseFields(base, baseLen, pflags, b) || !parseFields(cur, curLen, pflags, c) || b.num != c.num)
    {
        return -1;
    }
    za::U32 mask = 0;
    za::U32 zig[wireFields]{};
    for(int m = 0; m < wireFields; m++)
    {
        const int f = fieldOrder[m];
        if(!(c.bits & static_cast<za::U32>(wireBit[f])))
        {
            continue;
        }
        zig[m] = fieldChange(b.v[f], c.v[f], fieldSize(f, pflags));
        mask |= zig[m] != 0 ? 1u << m : 0u;
    }
    const bool tailSame = b.tailLen == c.tailLen && memcmp(b.tail, c.tail, static_cast<size_t>(c.tailLen)) == 0;
    mask |= tailSame ? 0u : maskTail;
    mask |= b.bits != c.bits ? maskBits : 0u;
    int at = putVarint(out, mask);
    if(mask & maskBits)
    {
        at += putVarint(out + at, b.bits ^ c.bits);
    }
    for(int m = 0; m < wireFields; m++)
    {
        if(mask & (1u << m))
        {
            at += putVarint(out + at, zig[m]);
        }
    }
    if(!tailSame)
    {
        const bool delta = b.tailLen == c.tailLen && bytesDelta(nullptr, b.tail, c.tail, c.tailLen) < c.tailLen;
        at += putVarint(out + at, static_cast<za::U32>(c.tailLen) << 1 | (delta ? 1u : 0u));
        if(delta)
        {
            za::U8 tmp[maxDeltaLen + maxDeltaLen / 8 + maxDeltaLen / 64]{};
            sizebuf_t sb{};
            sb.data = tmp;
            sb.maxsize = static_cast<int>(sizeof(tmp));
            bytesDelta(&sb, b.tail, c.tail, c.tailLen);
            if(at + sb.cursize > fieldsMost)
            {
                return -1;
            }
            memcpy(out + at, tmp, static_cast<size_t>(sb.cursize));
            at += sb.cursize;
        }
        else
        {
            if(at + c.tailLen > fieldsMost)
            {
                return -1;
            }
            memcpy(out + at, c.tail, static_cast<size_t>(c.tailLen));
            at += c.tailLen;
        }
    }
    return at;
}

// fieldsEncode's (`d`, at most `len` bytes) against `base` into `out` (fieldsMost): the update's length, -1 when malformed;
// `used`: the bytes read.
[[nodiscard]] int fieldsDecode(const za::U8* base, int baseLen, const za::U8* d, int len, unsigned pflags, za::U8* out, int& used)
{
    EntFields e;
    if(!parseFields(base, baseLen, pflags, e))
    {
        return -1;
    }
    int at = 0;
    za::U32 mask = 0, x = 0;
    if(!getVarint(d, len, at, mask) || mask >= (maskBits << 1))
    {
        return -1;
    }
    if((mask & maskBits) && !(getVarint(d, len, at, x) && ((e.bits ^= x), true)))
    {
        return -1;
    }
    if(!(e.bits & U_SIGNAL))
    {
        return -1;
    }
    za::U32 old[wireFields];
    memcpy(old, e.v, sizeof(old));
    for(int m = 0; m < wireFields; m++)
    {
        const int f = fieldOrder[m];
        const bool present = (e.bits & static_cast<za::U32>(wireBit[f])) != 0;
        e.v[f] = present ? old[f] : 0;
        if(!(mask & (1u << m)))
        {
            continue;
        }
        za::U32 zig = 0;
        if(!present || !getVarint(d, len, at, zig))
        {
            return -1;
        }
        e.v[f] = fieldApply(old[f], zig, fieldSize(f, pflags));
    }
    za::U8 tail[maxDeltaLen];
    if(mask & maskTail)
    {
        za::U32 head = 0;
        if(!getVarint(d, len, at, head) || (head >> 1) > static_cast<za::U32>(maxDeltaLen))
        {
            return -1;
        }
        const int tl = static_cast<int>(head >> 1);
        if(head & 1u)
        {
            if(tl != e.tailLen)
            {
                return -1;
            }
            memcpy(tail, e.tail, static_cast<size_t>(tl));
            // (bytesDelta's layout, read from `d`)
            const int groups = (tl + 7) / 8, tops = (groups + 7) / 8;
            if(at + tops > len)
            {
                return -1;
            }
            const za::U8* top = d + at;
            at += tops;
            for(int g = 0; g < groups; g++)
            {
                if(!(top[g / 8] & (1 << (g % 8))))
                {
                    continue;
                }
                if(at >= len)
                {
                    return -1;
                }
                const int gm = d[at++];
                for(int k = 0; k < 8; k++)
                {
                    if(!(gm & (1 << k)))
                    {
                        continue;
                    }
                    if(g * 8 + k >= tl || at >= len)
                    {
                        return -1;
                    }
                    tail[g * 8 + k] = d[at++];
                }
            }
        }
        else
        {
            if(at + tl > len)
            {
                return -1;
            }
            memcpy(tail, d + at, static_cast<size_t>(tl));
            at += tl;
        }
        e.tail = tail;
        e.tailLen = tl;
    }
    used = at;
    return writeFields(e, pflags, out, fieldsMost);
}

// fieldsEncode's `size` bytes in fieldsBuf decode against `base` to `cur` exactly (else it is not sent: B12b's safety).
[[nodiscard]] bool fieldsVerify(const za::U8* base, int baseLen, const za::U8* cur, int curLen, int size, unsigned pflags)
{
    int used = 0;
    const int n = fieldsDecode(base, baseLen, fieldsBuf, size, pflags, fieldsCheck, used);
    return n == curLen && used == size && memcmp(fieldsCheck, cur, static_cast<size_t>(n)) == 0;
}

} // namespace

namespace
{

// encode's first pass: each record that differs from the base's (in key order), its op and its size (without its head)
struct Entry
{
    za::I32 key{0}, op{0}, size{0}, ci{-1}; // ci: its record in `cur` (-1: gone)
    float prio{0.f};
    bool chosen{true};
};
za::Vector<Entry> entries;   // (encode's scratch)
za::Vector<za::I32> byPrio;  // (... the entries in priority order)
constexpr int headMost = 3;  // an entry's head: a key's step under 2^19
za::Vector<int> poseEntries; // the client: the pose entries read of each entity (vr_net_bytes poses; the rate's priorities)
za::Vector<float> poseDists; // ... and the sum of its distances from us at each (their mean: where it was meanwhile)
long long entOps[4][2]{};    // the client: the entities' entries read by op (gone, whole, delta, fields): count, bytes

} // namespace

int encode(const Snapshot* base, const Snapshot& cur, sizebuf_t* body, int budget, Snapshot& sent, unsigned pflags,
    const float* prio, za::U8* unsent, int soft)
{
    sent.records.clear();
    sent.bytes.clear();
    budget = za::min(budget, body->maxsize);
    soft = soft < 0 ? budget : za::min(soft, budget);
    const za::SizeT bn = base ? base->records.size() : 0, cn = cur.records.size();

    // What differs from the base, and whether all of it fits; if not, which entries go: in key order until one does
    // not fit (no priorities: the rest wait), or the ones that must (gone entries, priority infinite) and then the
    // highest priorities that fit.
    entries.clear();
    int need = body->cursize;
    for(za::SizeT bi = 0, ci = 0; bi < bn || ci < cn;)
    {
        const Record* b = bi < bn ? &base->records[bi] : nullptr;
        const Record* c = ci < cn ? &cur.records[ci] : nullptr;
        Entry e;
        if(c && (!b || c->key < b->key))
        {
            e = Entry{c->key, opWhole, varintSize(static_cast<za::U32>(c->len)) + c->len, static_cast<za::I32>(ci)};
            ci++;
        }
        else if(!c || b->key < c->key)
        {
            e = Entry{b->key, opGone, 0, -1};
            bi++;
        }
        else
        {
            bi++;
            ci++;
            if(b->len == c->len && memcmp(base->data(*b), cur.data(*c), static_cast<size_t>(c->len)) == 0)
            {
                continue;
            }
            const int wholeSize = varintSize(static_cast<za::U32>(c->len)) + c->len;
            const int deltaSize = b->len == c->len && c->len <= maxDeltaLen ? bytesDelta(nullptr, base->data(*b), cur.data(*c), c->len)
                                                                            : wholeSize + 1;
            // (an entity's update: by its fields too, when its decoding gives it back exactly; the smallest goes)
            int fieldsSize = keyClass(c->key) == 1 ? fieldsEncode(base->data(*b), b->len, cur.data(*c), c->len, pflags, fieldsBuf) : -1;
            fieldsSize = fieldsSize >= 0 && fieldsVerify(base->data(*b), b->len, cur.data(*c), c->len, fieldsSize, pflags) ? fieldsSize
                                                                                                                      : wholeSize + 1;
            const int ci1 = static_cast<za::I32>(ci - 1);
            e = fieldsSize < deltaSize && fieldsSize < wholeSize ? Entry{c->key, opFields, fieldsSize, ci1}
                : deltaSize < wholeSize                          ? Entry{c->key, opDelta, deltaSize, ci1}
                                                                 : Entry{c->key, opWhole, wholeSize, ci1};
        }
        e.prio = e.ci < 0 ? INFINITY : prio ? prio[e.ci] : 0.f;
        need += headMost + e.size;
        entries.emplaceBack(e);
    }
    if(need > soft)
    {
        int used = body->cursize;
        if(!prio)
        {
            bool cut = false;
            for(Entry& e : entries)
            {
                cut = cut || used + headMost + e.size > soft;
                e.chosen = !cut;
                used += e.chosen ? headMost + e.size : 0;
            }
        }
        else
        {
            byPrio.clear();
            for(za::SizeT i = 0; i < entries.size(); i++)
            {
                byPrio.emplaceBack(static_cast<za::I32>(i));
            }
            std::stable_sort(byPrio.begin(), byPrio.end(), [](za::I32 a, za::I32 b) {
                return entries[static_cast<za::SizeT>(a)].prio > entries[static_cast<za::SizeT>(b)].prio;
            });
            for(const za::I32 i : byPrio)
            {
                Entry& e = entries[static_cast<za::SizeT>(i)];
                e.chosen = used + headMost + e.size <= (e.prio == INFINITY ? budget : soft);
                used += e.chosen ? headMost + e.size : 0;
            }
        }
    }

    // Written in key order: the chosen entries (each as long as its real head fits too); the others keep the base's
    // record on both sides (a new one: none yet), `unsent` marks their records.
    int written = 0, prevKey = -1;
    za::SizeT bi = 0, ci = 0;
    for(const Entry& e : entries)
    {
        // (the records before it: unchanged)
        for(; ci < cn && cur.records[ci].key < e.key; ci++)
        {
            addFrom(sent, cur, cur.records[ci]);
        }
        for(; bi < bn && base->records[bi].key < e.key; bi++)
        {
        }
        const Record* b = bi < bn && base->records[bi].key == e.key ? &base->records[bi] : nullptr;
        const Record* c = ci < cn && cur.records[ci].key == e.key ? &cur.records[ci] : nullptr;
        bi += b ? 1 : 0;
        ci += c ? 1 : 0;
        const za::U32 head = entryHead(e.key, prevKey, e.op);
        if(!e.chosen || body->cursize + varintSize(head) + e.size > budget)
        {
            if(b)
            {
                addFrom(sent, *base, *b);
            }
            if(c && unsent)
            {
                unsent[ci - 1] = 1;
            }
            continue;
        }
        netmove::writeVarint(body, head);
        if(e.op == opDelta)
        {
            bytesDelta(body, base->data(*b), cur.data(*c), c->len);
        }
        else if(e.op == opFields)
        {
            const int n = fieldsEncode(base->data(*b), b->len, cur.data(*c), c->len, pflags, fieldsBuf);
            SZ_Write(body, fieldsBuf, n); // (the size the first pass measured and checked)
        }
        else if(e.op == opWhole)
        {
            netmove::writeVarint(body, static_cast<za::U32>(c->len));
            SZ_Write(body, cur.data(*c), c->len);
        }
        if(c)
        {
            addFrom(sent, cur, *c);
        }
        prevKey = e.key;
        written++;
    }
    for(; ci < cn; ci++)
    {
        addFrom(sent, cur, cur.records[ci]);
    }
    return written;
}

bool decode(const Snapshot* base, int bodyLen, Snapshot& out, unsigned pflags, long long* tally)
{
    out.records.clear();
    out.bytes.clear();
    const int end = msg_readcount + bodyLen;
    if(bodyLen < 0 || end > net_message.cursize)
    {
        return false;
    }
    za::SizeT bi = 0;
    const za::SizeT bn = base ? base->records.size() : 0;
    int prevKey = -1;
    int tallyClass = 0, tallyAt = msg_readcount, tallyOp = -1;
    while(msg_readcount < end)
    {
        // (a key's step is at most the last key + 1, from none: the predict record alone in a delta was refused
        // under a limit of 2 x avatarKey -- mp_loss's undecodable snapshots, B8b)
        const int entryAt = msg_readcount;
        za::U32 head;
        if(!netmove::readVarint(head) || (head >> 2) == 0 ||
            (head >> 2) > static_cast<za::U32>(keyLimit) || prevKey + static_cast<int>(head >> 2) >= keyLimit)
        {
            return false;
        }
        const int key = prevKey + static_cast<int>(head >> 2);
        const int op = static_cast<int>(head & 3u);
        prevKey = key;
        if(tally)
        {
            tally[tallyClass] += entryAt - tallyAt; // (the entry before: its bytes to its class)
            if(tallyClass == 1 && tallyOp >= 0)
            {
                entOps[tallyOp][0]++;
                entOps[tallyOp][1] += entryAt - tallyAt;
            }
            tallyClass = keyClass(key);
            tallyOp = static_cast<int>(head & 3u);
            tallyAt = entryAt;
            const int e = key - poseKey;
            if(op != opGone && e >= 0 && e < 65536)
            {
                if(e >= static_cast<int>(poseEntries.size()))
                {
                    poseEntries.resize(static_cast<za::SizeT>(e) + 1, 0);
                    poseDists.resize(static_cast<za::SizeT>(e) + 1, 0.f);
                }
                poseEntries[static_cast<za::SizeT>(e)]++;
                if(e < cl.num_entities && cl.viewentity > 0 && cl.viewentity < cl.num_entities)
                {
                    const float* o = cl_entities[e].origin;
                    const float* me = cl_entities[cl.viewentity].origin;
                    poseDists[static_cast<za::SizeT>(e)] += sqrtf((o[0] - me[0]) * (o[0] - me[0]) + (o[1] - me[1]) * (o[1] - me[1]) +
                                                                  (o[2] - me[2]) * (o[2] - me[2]));
                }
            }
        }
        while(bi < bn && base->records[bi].key < key)
        {
            addFrom(out, *base, base->records[bi++]); // (unchanged)
        }
        const Record* b = bi < bn && base->records[bi].key == key ? &base->records[bi] : nullptr;
        bi += b ? 1 : 0;
        if(op == opGone)
        {
            if(!b)
            {
                return false;
            }
            continue;
        }
        if(op == opWhole)
        {
            za::U32 len;
            if(!netmove::readVarint(len) || len > static_cast<za::U32>(end - msg_readcount))
            {
                return false;
            }
            out.add(key, net_message.data + msg_readcount, static_cast<int>(len));
            msg_readcount += static_cast<int>(len);
            continue;
        }
        if(!b)
        {
            return false;
        }
        if(op == opFields)
        {
            int used = 0;
            const int n = fieldsDecode(base->data(*b), b->len, net_message.data + msg_readcount, end - msg_readcount, pflags,
                fieldsCheck, used);
            if(n < 0)
            {
                return false;
            }
            out.add(key, fieldsCheck, n);
            msg_readcount += used;
            continue;
        }
        const za::I32 at = static_cast<za::I32>(out.bytes.size());
        addFrom(out, *base, *b);
        if(!readBytesDelta(out.bytes.data() + at, b->len, end))
        {
            return false;
        }
    }
    while(bi < bn)
    {
        addFrom(out, *base, base->records[bi++]);
    }
    if(tally)
    {
        tally[tallyClass] += msg_readcount - tallyAt;
        if(tallyClass == 1 && tallyOp >= 0)
        {
            entOps[tallyOp][0]++;
            entOps[tallyOp][1] += msg_readcount - tallyAt;
        }
    }
    return msg_readcount == end && !msg_badread;
}

namespace
{

// ---- Server --------------------------------------------------------------------------------------------------------

// Per client slot: the snapshots sent (`ring` back, by sequence number), the newest it acknowledged, the counts.
struct ServerSlot
{
    za::U32 nextSeq{1}; // (kept over maps: a reset forgets the snapshots, so an old acknowledgement matches none)
    za::U32 acked{0};
    Snapshot snaps[ring];
    ServerCounts counts;
    // B12's rate (his vr_net_rate, B/s; 0 none): the bytes he may still be sent (a bucket filled at the rate, at most
    // a burst), and each record's wait (frames it changed and waited: entities, avatars, poses by entity) for its priority
    double tokens{0.0}, filledAt{-1.0};
    za::Vector<za::U16> waits;
};
za::Vector<ServerSlot> serverSlots;

constexpr double rateBurst = 0.1;  // s of his rate the bucket holds (at least a full datagram)
constexpr int netHeader = 8;       // NetQuake's datagram header (the bytes his link carries a datagram besides ours)
constexpr float nearUnits = 256.f; // a record's weight: 1 here, halving each this much farther ...
constexpr float aheadWeight = 2.f; // ... twice ahead of his view (within 60 degrees), twice again another player's

// A record's index in his waits (-1: his own state, never waiting): entities, avatars, poses by entity.
[[nodiscard]] int waitIndex(int key)
{
    const int cls = keyClass(key);
    const int ents = qcvm ? qcvm->max_edicts : 0;
    const int e = cls == 1 ? key : cls == 2 ? key - avatarKey : cls == 4 ? key - poseKey : -1;
    if(e < 0 || e >= ents)
    {
        return -1;
    }
    return (cls == 1 ? 0 : cls == 2 ? 1 : 2) * ents + e;
}

// A record's weight for the player at `eye` looking along `fwd`: nearer and ahead first.
[[nodiscard]] float weightOf(int key, const float eye[3], const float fwd[3])
{
    const int cls = keyClass(key);
    const int e = cls == 1 ? key : cls == 2 ? key - avatarKey : key - poseKey;
    if(e <= 0 || e >= qcvm->num_edicts)
    {
        return 1.f;
    }
    const edict_t* ed = EDICT_NUM(e);
    float d[3], len2 = 0.f, dot = 0.f;
    for(int k = 0; k < 3; k++)
    {
        d[k] = (ed->v.absmin[k] + ed->v.absmax[k]) * 0.5f - eye[k];
        len2 += d[k] * d[k];
        dot += d[k] * fwd[k];
    }
    const float dist = sqrtf(len2);
    float w = nearUnits / (nearUnits + dist);
    w *= dot > 0.5f * dist ? aheadWeight : 1.f;
    w *= cls == 2 ? aheadWeight : 1.f;
    return w;
}

// The snapshot being made for the client whose datagram is being written (serverBegin .. serverEnd).
struct Building
{
    bool on{false};
    int slot{-1};
    client_t* client{nullptr};
    int rate{0};
    Snapshot cur;
    za::U8 body[MAX_DATAGRAM];
    za::Vector<float> prio;    // each record's priority (cur's order)
    za::Vector<za::U8> unsent; // each record that waited
    int chargeSlot{-1};        // the slot whose datagram serverCharge charges (serverEnd's)
};
Building building;

[[nodiscard]] ServerSlot* slotOf(int slot, bool make)
{
    if(slot < 0 || slot >= MAX_SCOREBOARD)
    {
        return nullptr;
    }
    if(slot >= static_cast<int>(serverSlots.size()))
    {
        if(!make)
        {
            return nullptr;
        }
        serverSlots.resize(static_cast<za::SizeT>(slot) + 1);
    }
    return &serverSlots[static_cast<za::SizeT>(slot)];
}

} // namespace

bool serverBegin(client_t* client)
{
    Building& b = building;
    b.on = false;
    if(vr_net_snapshots.value == 0.f || !client || !client->netconnection ||
        strcmp(NET_QSocketGetAddressString(client->netconnection), "LOCAL") == 0)
    {
        return false;
    }
    const int slot = static_cast<int>(client - svs.clients);
    if(!netmove::serverSends(slot) || !slotOf(slot, true))
    {
        return false; // (not a client of QVR_CLC_MOVES yet: NetQuake's datagram, as its moves are)
    }
    b.on = true;
    b.slot = slot;
    b.client = client;
    b.cur.clear();
    // (his rate: his own preference; the bucket filled by the game's time since his last datagram)
    b.rate = client->edict ? za::max(0, static_cast<int>(clientprefs::value(clientprefs::indexOf(&vr_net_rate), &vr_net_rate, client->edict)))
                           : 0;
    ServerSlot& s = *slotOf(slot, true);
    const double now = qcvm->time;
    if(b.rate > 0)
    {
        const double most = za::max(static_cast<double>(b.rate) * rateBurst, static_cast<double>(DATAGRAM_MTU + netHeader));
        s.tokens = s.filledAt < 0.0 || now < s.filledAt ? most : za::min(most, s.tokens + b.rate * (now - s.filledAt));
    }
    s.filledAt = now;
    return true;
}

void serverCharge(int bytes)
{
    Building& b = building;
    ServerSlot* s = slotOf(b.chargeSlot, false);
    b.chargeSlot = -1;
    if(s && b.rate > 0)
    {
        s->tokens -= bytes + netHeader;
    }
}

bool serverRecording()
{
    return building.on;
}

void serverTake(sizebuf_t* msg, int at, int key)
{
    if(!building.on || at < 0 || at > msg->cursize)
    {
        return;
    }
    building.cur.add(key, msg->data + at, msg->cursize - at);
    msg->cursize = at;
}

void serverRecord(int key, const za::U8* data, int len)
{
    if(building.on && len > 0)
    {
        building.cur.add(key, data, len);
    }
}

void serverEnd(sizebuf_t* msg)
{
    Building& bl = building;
    if(!bl.on)
    {
        return;
    }
    bl.on = false;
    ServerSlot* s = slotOf(bl.slot, true);
    const int room = msg->maxsize - msg->cursize - 1 - headerMost;
    if(!s || room <= 0)
    {
        return;
    }
    const za::U32 seq = s->nextSeq++;
    const Snapshot* base = nullptr;
    if(s->acked != 0 && seq - s->acked < static_cast<za::U32>(ring) && s->snaps[s->acked % ring].seq == s->acked)
    {
        base = &s->snaps[s->acked % ring];
    }
    bl.cur.sortByKey();
    Snapshot& sent = s->snaps[seq % ring];
    sizebuf_t body{};
    body.data = bl.body;
    body.maxsize = static_cast<int>(sizeof(bl.body));
    const za::SizeT before = bl.cur.records.size();

    // Its budget (B12): the room left after what is written and the broadcast to come (its events before poses, as when
    // the poses came after it; at most half), and under his rate what his bucket holds after them.
    const int broadcast = za::min(sv.datagram.cursize, room / 2);
    int budget = room - broadcast;
    if(bl.rate > 0)
    {
        budget = za::min(budget, static_cast<int>(s->tokens) - msg->cursize - broadcast - netHeader - 6);
    }
    budget = za::max(budget, 0);
    // Each record's priority: his own state always first; the others by weight (near, ahead, a player) times their wait.
    const za::SizeT n = bl.cur.records.size();
    bl.prio.resize(n);
    bl.unsent.clear();
    bl.unsent.resize(n, 0);
    const int ents = qcvm->max_edicts;
    if(static_cast<int>(s->waits.size()) != 3 * ents)
    {
        s->waits.clear();
        s->waits.resize(static_cast<za::SizeT>(3 * ents), 0);
    }
    float eye[3]{}, fwd[3]{1.f, 0.f, 0.f};
    if(const edict_t* p = bl.client ? bl.client->edict : nullptr)
    {
        vec3_t right, up;
        AngleVectors(const_cast<float*>(p->v.v_angle), fwd, right, up);
        for(int k = 0; k < 3; k++)
        {
            eye[k] = p->v.origin[k] + p->v.view_ofs[k];
        }
    }
    for(za::SizeT i = 0; i < n; i++)
    {
        const int key = bl.cur.records[i].key;
        const int wi = waitIndex(key);
        bl.prio[i] = keyClass(key) == 0 || keyClass(key) == 3 ? INFINITY
                                                              : weightOf(key, eye, fwd) * (1.f + (wi >= 0 ? s->waits[static_cast<za::SizeT>(wi)] : 0));
    }
    encode(base, bl.cur, &body, room, sent, sv.protocolflags, bl.prio.data(), bl.unsent.data(), za::min(budget, room));
    int waited = 0;
    for(za::SizeT i = 0; i < n; i++)
    {
        const int wi = waitIndex(bl.cur.records[i].key);
        if(wi >= 0)
        {
            za::U16& w = s->waits[static_cast<za::SizeT>(wi)];
            w = bl.unsent[i] ? static_cast<za::U16>(za::min(static_cast<int>(w) + 1, 60000)) : za::U16{0};
        }
        waited += bl.unsent[i];
    }
    bl.chargeSlot = bl.slot;
    sent.seq = seq;
    MSG_WriteByte(msg, protocol::svc_quakevr);
    MSG_WriteByte(msg, protocol::QVR_SVC_SNAPSHOT);
    const int at = msg->cursize;
    netmove::writeVarint(msg, seq);
    netmove::writeVarint(msg, base ? seq - base->seq : 0u);
    netmove::writeVarint(msg, static_cast<za::U32>(body.cursize));
    SZ_Write(msg, body.data, body.cursize);
    ServerCounts& c = s->counts;
    c.sent++;
    (base ? c.delta : c.full)++;
    c.cut += body.cursize >= room - 8 && sent.records.size() != before ? 1 : 0;
    c.waited += waited;
    c.waitFrames += waited > 0 ? 1 : 0;
    c.rate = bl.rate;
    c.bytes += msg->cursize - at + 2;
    c.seq = seq;
    c.acked = s->acked;
}

void serverAck(int slotNum, za::U32 seq)
{
    ServerSlot* s = slotOf(slotNum, false);
    if(s && seq == 0)
    {
        s->acked = 0; // (it has none: a new connection or map, or a demo begun; the next whole)
    }
    else if(s && seq > s->acked && seq < s->nextSeq && s->snaps[seq % ring].seq == seq)
    {
        s->acked = seq;
    }
}

void serverReset(int slotNum, bool keepCounts)
{
    if(ServerSlot* s = slotOf(slotNum, false))
    {
        s->acked = 0;
        for(Snapshot& sn : s->snaps)
        {
            sn.clear();
        }
        if(!keepCounts)
        {
            s->counts = {};
        }
    }
}

ServerCounts serverCounts(int slotNum)
{
    const ServerSlot* s = slotOf(slotNum, false);
    return s ? s->counts : ServerCounts{};
}

namespace
{

// ---- Client --------------------------------------------------------------------------------------------------------

struct ClientState
{
    Snapshot snaps[ring];
    za::U32 last{0}; // the newest read (acknowledged in our moves)
    bool wantsWhole{false}; // a demo begun: 0 acknowledged until a whole one comes (the demo's start, vr_netsnap.hpp)
    ClientCounts counts;
};
ClientState clientState;

// What this client read, by kind (noteMessage, noteCommand; B12's byte table).
struct ByteCounts
{
    long long reliable{0}, unreliable{0}, messages{0};
    long long svc[256]{};       // the unreliable messages' bytes by command (an entity's update: U_SIGNAL's)
    long long qvr[256]{};       // ... svc_quakevr's by its sub-command
    long long snap[keyClasses]{}; // a snapshot's entries by their keys' class (its own header: the rest of qvr's)
    bool reliableMsg{false};
    int openAt{-1}, openCmd{0}, openSub{0}; // the command being read: where it began
};
ByteCounts byteCounts;

// ---- The interpolation buffer ----------------------------------------------------------------------------------------

constexpr int historyLen = 8;           // messages kept per entity
constexpr double tick = 1.0 / 72.0;     // the server's frame
constexpr double delayMin = 1.5 * tick; // the delay's least ...
constexpr double delayMost = 0.25;      // ... and most
constexpr double delayMargin = 0.5 * tick;
constexpr double peakDecay = 0.01;      // s a second: the longest gap's hold on the delay fades this fast
constexpr double clockGain = 4.0;       // the delay's clock: this much faster a second behind (slower ahead) ...
constexpr double rateLeast = 0.85, rateMost = 1.15; // ... within these
constexpr double snapAfter = 0.25;      // s off its target: set there (a new map, a long stall)
constexpr double cushion = 0.5 * tick;  // the draw time this near the newest message: its clock slows, to a stop at it

struct Sample
{
    double t{0.0}; // the message's server time
    float origin[3]{};
    float angles[3]{};
};
struct History
{
    Sample s[historyLen];
    int n{0}, head{0};
    const qmodel_t* model{nullptr};
    za::U32 seq{0}; // the snapshot of its newest
};
struct Interp
{
    za::Vector<History> of; // by entity number
    bool valid{false};
    double est{0.0};      // the newest message's server time, as a clock (following the messages as they come)
    double rt{0.0};       // the time the others are drawn at: est - the delay, its rate eased
    double lastReal{0.0};
    double newest{0.0};   // the newest message's time
    double peak{0.0};     // the longest recent gap: how far past the newest the clock was when a message came
    double delay{0.0};
    int holds{0};         // frames the draw time slowed near the newest message (a gap longer than the delay)
};
Interp interp;

void interpArrive(double t)
{
    Interp& ip = interp;
    if(!ip.valid || za::abs(t - ip.est) > snapAfter)
    {
        ip.valid = true;
        ip.est = t;
        ip.newest = t;
        ip.peak = 2.0 * tick;
        ip.delay = ip.peak + delayMargin;
        ip.rt = t - ip.delay;
        ip.lastReal = realtime;
        return;
    }
    ip.peak = za::max(ip.peak, ip.est - ip.newest);
    ip.est += (t - ip.est) * 0.1; // (a nudge: the arrivals' jitter averaged)
    ip.newest = za::max(ip.newest, t);
}

// Entity `e`'s place in snapshot `seq` (the one read before it: `prevSeq`) into its history.
void interpNote(int e, za::U32 prevSeq, za::U32 seq)
{
    Interp& ip = interp;
    if(e <= 0 || e >= cl.num_entities)
    {
        return;
    }
    if(e >= static_cast<int>(ip.of.size()))
    {
        ip.of.resize(static_cast<za::SizeT>(e) + 1);
    }
    const entity_t& ent = cl_entities[e];
    History& h = ip.of[static_cast<za::SizeT>(e)];
    if(h.seq != prevSeq || prevSeq == 0 || ent.model != h.model)
    {
        h.n = 0; // (new, or back in sight, or another thing in its slot: its history starts again)
    }
    h.model = ent.model;
    h.seq = seq;
    Sample& s = h.s[h.head];
    s.t = cl.mtime[0];
    memcpy(s.origin, ent.msg_origins[0], sizeof(s.origin));
    memcpy(s.angles, ent.msg_angles[0], sizeof(s.angles));
    h.head = (h.head + 1) % historyLen;
    h.n = za::min(h.n + 1, historyLen);
}

// The snapshot's records parsed as the server's messages they are (net_message pointed at each); its entities' places
// into their histories when it is a new one (`prevSeq`: the one read before it; else 0 and seq 0).
void apply(const Snapshot& s, za::U32 prevSeq, za::U32 seq)
{
    ClientCounts& counts = clientState.counts;
    const sizebuf_t savedMsg = net_message;
    const int savedCount = msg_readcount;
    const qboolean savedBad = msg_badread;
    for(const Record& r : s.records)
    {
        net_message.data = const_cast<byte*>(s.data(r));
        net_message.cursize = r.len;
        net_message.maxsize = r.len;
        msg_readcount = 0;
        msg_badread = false;
        while(msg_readcount < r.len && !msg_badread)
        {
            const int cmd = MSG_ReadByte();
            if(cmd & U_SIGNAL)
            {
                CL_ParseUpdate(cmd & 127);
            }
            else if(cmd == svc_clientdata)
            {
                CL_ParseClientdata();
            }
            else if(!VR_ParseServerMessage(cmd))
            {
                msg_badread = true;
            }
        }
        counts.bad += msg_badread || msg_readcount != r.len ? 1 : 0;
    }
    net_message = savedMsg;
    msg_readcount = savedCount;
    msg_badread = savedBad;
    if(seq == 0)
    {
        return;
    }
    for(const Record& r : s.records)
    {
        if(r.key > clientdataKey && r.key < avatarKey)
        {
            interpNote(r.key, prevSeq, seq);
        }
    }
}

} // namespace

za::U32 clientAck()
{
    return clientState.wantsWhole ? 0u : clientState.last;
}

void clientWantWhole()
{
    clientState.wantsWhole = clientState.last != 0;
}

void parse()
{
    za::U32 seq, dist, len;
    if(!netmove::readVarint(seq) || !netmove::readVarint(dist) || !netmove::readVarint(len) ||
        len > static_cast<za::U32>(net_message.cursize - msg_readcount))
    {
        msg_badread = true; // (CL_ParseServerMessage: a bad server message)
        return;
    }
    ClientState& c = clientState;
    const int bodyStart = msg_readcount;
    bool ok = seq > c.last && dist < static_cast<za::U32>(ring);
    const Snapshot* base = nullptr;
    if(ok && dist != 0)
    {
        base = &c.snaps[(seq - dist) % ring];
        ok = seq > dist && base->seq == seq - dist;
    }
    Snapshot& out = c.snaps[seq % ring];
    ok = ok && decode(base, static_cast<int>(len), out, cl.protocolflags, byteCounts.snap);
    msg_readcount = bodyStart + static_cast<int>(len);
    msg_badread = false;
    if(!ok)
    {
        c.counts.undecodable++;
        Con_Printf("netsnap: snapshot %u undecodable (base %u back, newest read %u, base kept %u, len %u)\n", seq, dist, c.last,
            base ? base->seq : 0u, len);
        if(seq > c.last)
        {
            out.seq = 0;
        }
        // (the entities kept as the last read: none vanishes for a message)
        if(c.last != 0 && c.snaps[c.last % ring].seq == c.last)
        {
            apply(c.snaps[c.last % ring], 0, 0);
        }
        return;
    }
    out.seq = seq;
    const za::U32 prev = c.last;
    c.last = seq;
    c.wantsWhole = c.wantsWhole && dist != 0;
    c.counts.decoded++;
    c.counts.records += static_cast<int>(out.records.size());
    c.counts.bytes += static_cast<long long>(len);
    apply(out, prev, seq);
    interpArrive(cl.mtime[0]);
}

void clientClear()
{
    ClientState& c = clientState;
    c.last = 0;
    for(Snapshot& s : c.snaps)
    {
        s.clear();
    }
    interp.valid = false;
    for(History& h : interp.of)
    {
        h.n = 0;
        h.model = nullptr;
    }
}

ClientCounts clientCounts()
{
    return clientState.counts;
}

void noteMessage(int kind)
{
    ByteCounts& b = byteCounts;
    b.reliableMsg = kind == 1;
    b.openAt = -1;
    b.messages++;
    (b.reliableMsg ? b.reliable : b.unreliable) += net_message.cursize;
}

void noteCommand(int at)
{
    ByteCounts& b = byteCounts;
    if(b.openAt >= 0 && at > b.openAt && !b.reliableMsg)
    {
        (b.openCmd == protocol::svc_quakevr ? b.qvr[b.openSub] : b.svc[b.openCmd]) += at - b.openAt;
    }
    b.openAt = -1;
    if(at >= 0 && at < net_message.cursize)
    {
        b.openAt = at;
        b.openCmd = (net_message.data[at] & U_SIGNAL) ? U_SIGNAL : net_message.data[at];
        b.openSub = at + 1 < net_message.cursize ? net_message.data[at + 1] : 0;
    }
}

void bytesReset()
{
    byteCounts = ByteCounts{};
    memset(entOps, 0, sizeof(entOps));
}

// "mp_netbytes:" the bytes read since the connection (or `vr_net_bytes reset`): reliable and unreliable, the unreliable
// by kind (the snapshot's entries by class: its player state, entities, avatars, walk state, poses; its header; the
// poses, avatars and entities outside it; sounds, temp entities, the rest), then the three biggest of the rest, then the
// snapshot's entity entries by op (ent_ops: gone, whole, byte by byte, by fields; each count/bytes).
void bytesReport()
{
    const ByteCounts& b = byteCounts;
    long long snapEntries = 0;
    for(const long long n : b.snap)
    {
        snapEntries += n;
    }
    const long long snap = b.qvr[protocol::QVR_SVC_SNAPSHOT], pose = b.qvr[protocol::QVR_SVC_ARMPOSE],
                    avatar = b.qvr[protocol::QVR_SVC_AVATAR], ent = b.svc[U_SIGNAL], cd = b.svc[svc_clientdata],
                    sound = b.svc[svc_sound], tent = b.svc[svc_temp_entity];
    const long long rest = b.unreliable - snap - pose - avatar - ent - cd - sound - tent;
    // (the rest's three biggest: "q<sub>" a svc_quakevr's, else the command's number)
    int top[3] = {-1, -1, -1};
    long long topN[3] = {0, 0, 0};
    for(int i = 0; i < 512; i++)
    {
        const bool q = i >= 256;
        const int c = i & 255;
        if((q && (c == protocol::QVR_SVC_SNAPSHOT || c == protocol::QVR_SVC_ARMPOSE || c == protocol::QVR_SVC_AVATAR)) ||
            (!q && (c == U_SIGNAL || c == svc_clientdata || c == svc_sound || c == svc_temp_entity || c == protocol::svc_quakevr)))
        {
            continue;
        }
        const long long n = q ? b.qvr[c] : b.svc[c];
        for(int k = 0; k < 3; k++)
        {
            if(n > topN[k])
            {
                for(int m = 2; m > k; m--)
                {
                    top[m] = top[m - 1];
                    topN[m] = topN[m - 1];
                }
                top[k] = i;
                topN[k] = n;
                break;
            }
        }
    }
    Con_Printf("mp_netbytes: messages=%lld reliable=%lld unreliable=%lld snap=%lld snap_cd=%lld snap_ent=%lld snap_av=%lld "
               "snap_pred=%lld snap_pose=%lld snap_hdr=%lld pose=%lld avatar=%lld ent=%lld cd=%lld sound=%lld tent=%lld "
               "rest=%lld top=%s%d:%lld,%s%d:%lld,%s%d:%lld ent_ops=%lld/%lld,%lld/%lld,%lld/%lld,%lld/%lld\n",
        b.messages, b.reliable, b.unreliable, snap, b.snap[0], b.snap[1], b.snap[2], b.snap[3], b.snap[4],
        snap - snapEntries, pose, avatar, ent, cd, sound, tent, rest, top[0] >= 256 ? "q" : "", top[0] & 255, topN[0],
        top[1] >= 256 ? "q" : "", top[1] & 255, topN[1], top[2] >= 256 ? "q" : "", top[2] & 255, topN[2], entOps[0][0], entOps[0][1],
        entOps[1][0], entOps[1][1], entOps[2][0], entOps[2][1], entOps[3][0], entOps[3][1]);
}

bool interpActive()
{
    return interp.valid && vr_net_interp.value != 0.f && clientState.last != 0 && cls.state == ca_connected &&
           !cls.demoplayback;
}

void interpFrame()
{
    Interp& ip = interp;
    if(!interpActive())
    {
        ip.lastReal = realtime;
        return;
    }
    const double dt = za::clamp(realtime - ip.lastReal, 0.0, 0.1);
    ip.lastReal = realtime;
    ip.est += dt;
    ip.peak = za::max(ip.peak - dt * peakDecay, tick);
    ip.delay = za::clamp(ip.peak + delayMargin, delayMin, delayMost);
    const double target = ip.est - ip.delay;
    const double err = target - ip.rt;
    if(za::abs(err) > snapAfter)
    {
        ip.rt = target;
    }
    else
    {
        // Near the newest message (a gap longer than the delay: lost packets in a row, a late one) the clock slows to
        // a stop as it nears it rather than running past and holding: past it, the entities stood at the newest place
        // and the next message's lerp put them a gap's motion further at once (B8c: mp_loss's "jumps" at 10% loss).
        // They slow down smoothly instead and the clock's rate catches the delay up after.
        const double room = za::clamp((ip.newest - ip.rt) / cushion, 0.0, 1.0);
        ip.holds += room < 1.0 ? 1 : 0;
        ip.rt += dt * za::clamp(1.0 + err * clockGain, rateLeast, rateMost) * room;
    }
}

bool interpEntity(int num, float origin[3], float angles[3])
{
    const Interp& ip = interp;
    if(!interpActive() || num == cl.viewentity || num <= 0 || num >= static_cast<int>(ip.of.size()))
    {
        return false;
    }
    const entity_t& ent = cl_entities[num];
    if(ent.model && ent.model->type == mod_brush)
    {
        return false; // (doors and lifts: in step with the player's own entity, NetQuake's lerp: he rides them)
    }
    const History& h = ip.of[static_cast<za::SizeT>(num)];
    if(h.n == 0 || h.model != ent.model)
    {
        return false;
    }
    const Sample* newer = nullptr;
    const Sample* older = nullptr;
    for(int k = 0; k < h.n; k++)
    {
        const Sample& s = h.s[(h.head - 1 - k + 2 * historyLen) % historyLen];
        if(s.t <= ip.rt)
        {
            older = &s;
            break;
        }
        newer = &s;
    }
    const Sample* only = !older ? newer : !newer ? older : nullptr; // (before the oldest, or past the newest: held)
    if(!only)
    {
        for(int j = 0; j < 3; j++)
        {
            if(za::abs(newer->origin[j] - older->origin[j]) > 100.f)
            {
                only = older; // (a teleport: no lerp across it; the newer when its time comes)
            }
        }
    }
    if(only)
    {
        memcpy(origin, only->origin, sizeof(only->origin));
        memcpy(angles, only->angles, sizeof(only->angles));
        return true;
    }
    const double span = newer->t - older->t;
    const float f = span > 0.0 ? static_cast<float>(za::clamp((ip.rt - older->t) / span, 0.0, 1.0)) : 1.f;
    for(int j = 0; j < 3; j++)
    {
        origin[j] = older->origin[j] + f * (newer->origin[j] - older->origin[j]);
    }
    const glm::vec3 a = mpavatar::slerpAngles(glm::vec3{older->angles[0], older->angles[1], older->angles[2]},
        glm::vec3{newer->angles[0], newer->angles[1], newer->angles[2]}, f);
    angles[0] = a.x;
    angles[1] = a.y;
    angles[2] = a.z;
    return true;
}

double interpTime()
{
    return interp.rt;
}

double interpDelayMs()
{
    return interp.delay * 1000.0;
}

int interpHolds()
{
    return interp.holds;
}

double interpNewest()
{
    return interp.newest;
}

bool interpSample(int num, int back, double& t, float origin[3])
{
    const Interp& ip = interp;
    if(num <= 0 || num >= static_cast<int>(ip.of.size()))
    {
        return false;
    }
    const History& h = ip.of[static_cast<za::SizeT>(num)];
    if(back < 0 || back >= h.n)
    {
        return false;
    }
    const Sample& s = h.s[(h.head - 1 - back + 2 * historyLen) % historyLen];
    t = s.t;
    memcpy(origin, s.origin, sizeof(s.origin));
    return true;
}

namespace
{

// vr_netsnap_selftest's scratch (off the stack).
struct SelftestScratch
{
    Snapshot base, cur, sent, read;
    byte data[4096];
};
SelftestScratch selftestScratch;
constexpr unsigned selftestFlags = PRFL_INT32COORD | PRFL_SHORTANGLE; // (Quake VR's server: PROTOCOL_RMQ's)

// B12b: an entity's update as sv_main.c writes it (Quake VR's flags) into `out`: its length. `tail` bytes of Quake VR's
// own after it (from `seed`).
int entityUpdate(za::U8* out, int bits, int num, const float* o, const float* a, int frame, int lerp, int tail, int seed)
{
    sizebuf_t sb{};
    sb.data = out;
    sb.maxsize = 128;
    bits |= num >= 256 ? U_LONGENTITY : 0;
    bits |= bits >= 65536 ? U_EXTEND1 : 0;
    bits |= bits >= 256 ? U_MOREBITS : 0;
    MSG_WriteByte(&sb, (bits & 0xFF) | U_SIGNAL);
    if(bits & U_MOREBITS)
    {
        MSG_WriteByte(&sb, (bits >> 8) & 0xFF);
    }
    if(bits & U_EXTEND1)
    {
        MSG_WriteByte(&sb, (bits >> 16) & 0xFF);
    }
    (bits & U_LONGENTITY) ? MSG_WriteShort(&sb, num) : MSG_WriteByte(&sb, num);
    if(bits & U_FRAME)
    {
        MSG_WriteByte(&sb, frame);
    }
    const int ob[3] = {U_ORIGIN1, U_ORIGIN2, U_ORIGIN3}, ab[3] = {U_ANGLE1, U_ANGLE2, U_ANGLE3};
    for(int i = 0; i < 3; i++)
    {
        if(bits & ob[i])
        {
            MSG_WriteCoord(&sb, o[i], selftestFlags);
        }
        if(bits & ab[i])
        {
            MSG_WriteAngle(&sb, a[i], selftestFlags);
        }
    }
    if(bits & U_LERPFINISH)
    {
        MSG_WriteByte(&sb, lerp);
    }
    for(int k = 0; k < tail; k++)
    {
        MSG_WriteByte(&sb, (seed * 13 + k * 5) & 0xFF);
    }
    return sb.cursize;
}

// A snapshot of `n` records (keys 0, 1, 3, 5...), record i `len` bytes from `seed`.
void fillSnapshot(Snapshot& s, int n, int len, int seed)
{
    s.clear();
    za::U8 buf[64]{};
    for(int i = 0; i < n; i++)
    {
        for(int k = 0; k < len && k < 64; k++)
        {
            buf[k] = static_cast<za::U8>((i * 31 + k * 7 + seed * (k == 3 ? 1 : 0)) & 0xFF);
        }
        s.add(i == 0 ? 0 : 2 * i - 1, buf, za::min(len, 64));
    }
    s.add(avatarKey + 2, buf, 40);
    s.sortByKey();
}

[[nodiscard]] bool sameSnapshot(const Snapshot& a, const Snapshot& b)
{
    if(a.records.size() != b.records.size())
    {
        return false;
    }
    for(za::SizeT i = 0; i < a.records.size(); i++)
    {
        const Record& x = a.records[i];
        const Record& y = b.records[i];
        if(x.key != y.key || x.len != y.len || memcmp(a.data(x), b.data(y), static_cast<size_t>(x.len)) != 0)
        {
            return false;
        }
    }
    return true;
}

// `cur` encoded against `base` (nullptr none) within `budget`, decoded back against it: the client's equals `sent`.
[[nodiscard]] bool roundTrip(const Snapshot* base, const Snapshot& cur, int budget, int& bytes)
{
    SelftestScratch& t = selftestScratch;
    sizebuf_t buf{};
    buf.data = t.data;
    buf.maxsize = sizeof(t.data);
    encode(base, cur, &buf, budget, t.sent, selftestFlags);
    bytes = buf.cursize;
    net_message = buf;
    MSG_BeginReading();
    return decode(base, buf.cursize, t.read, selftestFlags) && sameSnapshot(t.read, t.sent);
}

} // namespace

void selftest_f()
{
    SelftestScratch& t = selftestScratch;
    testkit::Tally tally;
    const auto check = [&](bool ok, const char* what) {
        tally.check(ok);
        Con_Printf("netsnap selftest: %s %s\n", ok ? "PASS" : "FAIL", what);
    };
    const sizebuf_t savedMsg = net_message;
    const int savedCount = msg_readcount;
    const qboolean savedBad = msg_badread;
    int fullBytes = 0, sameBytes = 0, deltaBytes = 0, cutBytes = 0;
    fillSnapshot(t.base, 40, 16, 0);
    const bool whole = roundTrip(nullptr, t.base, 4096, fullBytes) && sameSnapshot(t.sent, t.base);
    check(whole, va("a snapshot without a base: whole, read back the same (%d records, %d bytes)", 41, fullBytes));
    const bool same = roundTrip(&t.base, t.base, 4096, sameBytes) && sameSnapshot(t.sent, t.base);
    check(same && sameBytes == 0, va("the same again against it: read back the same in %d bytes", sameBytes));
    fillSnapshot(t.cur, 40, 16, 0);
    {
        // two records changed by a byte, one gone (key 5), one new (key 200), one longer (key 7)
        Snapshot& c = t.cur;
        c.bytes[c.records[1].at + 3] ^= 0x55;
        c.bytes[c.records[20].at + 9] ^= 0x01;
        Snapshot rebuilt;
        za::U8 buf[64]{};
        for(const Record& r : c.records)
        {
            if(r.key == 5)
            {
                continue;
            }
            rebuilt.add(r.key, r.key == 7 ? buf : c.data(r), r.key == 7 ? 20 : r.len);
        }
        rebuilt.add(200, buf, 12);
        rebuilt.sortByKey();
        c = rebuilt;
    }
    const bool changed = roundTrip(&t.base, t.cur, 4096, deltaBytes) && sameSnapshot(t.sent, t.cur);
    check(changed, va("changed, gone, new and resized records: read back the same in %d bytes (whole: %d)", deltaBytes,
                       fullBytes));
    check(deltaBytes > 0 && deltaBytes < 50, va("two one-byte changes, one gone, two whole (32 B): %d bytes", deltaBytes));
    {
        // the predict record (the last key) the only one changed: its step from none is the largest (B8b's undecodable)
        int lastBytes = 0;
        za::U8 buf[16]{};
        t.base.add(predictKey, buf, 16);
        t.base.sortByKey();
        t.cur = t.base;
        t.cur.bytes[t.cur.records[t.cur.records.size() - 1].at + 5] ^= 0x10;
        const bool lastOk = roundTrip(&t.base, t.cur, 4096, lastBytes) && sameSnapshot(t.sent, t.cur);
        check(lastOk, va("the predict record alone changed: read back the same in %d bytes", lastBytes));
    }
    const bool cutOk = roundTrip(nullptr, t.cur, 200, cutBytes);
    check(cutOk && cutBytes <= 200 && t.sent.records.size() < t.cur.records.size(),
        va("past a budget of 200 bytes: cut there (%d bytes, %d of %d records), the client's as the server keeps it", cutBytes,
            static_cast<int>(t.sent.records.size()), static_cast<int>(t.cur.records.size())));
    {
        // B12: a pose record (the largest keys) alone new, then changed: read back the same
        int poseBytes = 0;
        za::U8 buf[48]{};
        t.cur = t.base;
        t.cur.add(poseKey + 300, buf, 48);
        t.cur.sortByKey();
        bool poseOk = roundTrip(&t.base, t.cur, 4096, poseBytes) && sameSnapshot(t.sent, t.cur);
        t.base = t.cur;
        t.cur.bytes[t.cur.records[t.cur.records.size() - 1].at + 9] ^= 0x22;
        poseOk = poseOk && roundTrip(&t.base, t.cur, 4096, poseBytes) && sameSnapshot(t.sent, t.cur) && poseBytes < 10;
        check(poseOk, va("a pose record new, then a byte of it changed: read back the same (%d bytes)", poseBytes));
    }
    {
        // B12's rate: every record changed, a soft budget of 40 bytes: the player state (infinite priority) and the
        // highest priority go, the others wait as the base's on both sides; read back the same
        t.cur = t.base;
        const za::SizeT n = t.cur.records.size();
        float prio[64]{};
        za::U8 unsent[64]{};
        for(za::SizeT i = 0; i < n && i < 64; i++)
        {
            t.cur.bytes[t.cur.records[i].at] ^= 0x01;
            prio[i] = t.cur.records[i].key == clientdataKey ? INFINITY : i == 30 ? 10.f : 1.f;
        }
        sizebuf_t buf{};
        buf.data = t.data;
        buf.maxsize = sizeof(t.data);
        encode(&t.base, t.cur, &buf, 4096, t.sent, selftestFlags, prio, unsent, 40);
        const int bytes = buf.cursize;
        net_message = buf;
        MSG_BeginReading();
        bool rateOk = n <= 64 && decode(&t.base, buf.cursize, t.read, selftestFlags) && sameSnapshot(t.read, t.sent);
        int waited = 0;
        for(za::SizeT i = 0; i < n && i < 64; i++)
        {
            waited += unsent[i];
        }
        rateOk = rateOk && !unsent[0] && !unsent[30] && waited >= static_cast<int>(n) - 6 && bytes <= 40 &&
                 memcmp(t.sent.data(t.sent.records[30]), t.cur.data(t.cur.records[30]), 4) == 0 &&
                 memcmp(t.sent.data(t.sent.records[29]), t.base.data(t.base.records[29]), 4) == 0;
        check(rateOk, va("a rate's budget of 40 bytes: the player state and the highest priority sent, %d of %d waited (%d bytes)",
                           waited, static_cast<int>(n), bytes));
    }
    {
        // B12b: entity updates by their fields. A grunt (300: a long number) and its weapon (301) stepping: origins,
        // yaw, frame and lerp changed, the weapon's tail not; then the grunt's layout changed (its pitch now sent, its
        // lerp not): read back the same, in fewer bytes than byte by byte (the delta of the same, measured by encode)
        za::U8 u[4][128];
        const float o0[3] = {-260.f, 62.f, 24.f}, o1[3] = {-252.5f, 66.f, 24.f}, a0[3] = {0.f, 90.f, 0.f}, a1[3] = {0.f, 97.f, 0.f};
        const float wa0[3] = {10.f, 90.f, 5.f}, wa1[3] = {12.f, 96.f, 4.f};
        const int gb = U_ORIGIN1 | U_ORIGIN2 | U_ORIGIN3 | U_ANGLE2 | U_FRAME | U_STEP | U_LERPFINISH;
        const int wb = U_ORIGIN1 | U_ORIGIN2 | U_ORIGIN3 | U_ANGLE1 | U_ANGLE2 | U_ANGLE3;
        const int g0 = entityUpdate(u[0], gb, 300, o0, a0, 12, 25, 0, 0);
        const int w0 = entityUpdate(u[1], wb, 301, o0, wa0, 0, 0, 6, 1);
        const int g1 = entityUpdate(u[2], gb, 300, o1, a1, 13, 21, 0, 0);
        const int w1 = entityUpdate(u[3], wb, 301, o1, wa1, 0, 0, 6, 1);
        t.base.clear();
        t.base.add(300, u[0], g0);
        t.base.add(301, u[1], w0);
        t.cur.clear();
        t.cur.add(300, u[2], g1);
        t.cur.add(301, u[3], w1);
        int fieldBytes = 0, byteBytes = 0;
        bool ok = roundTrip(&t.base, t.cur, 4096, fieldBytes) && sameSnapshot(t.read, t.cur);
        byteBytes = 2 * 2 + bytesDelta(nullptr, u[0], u[2], g0) + bytesDelta(nullptr, u[1], u[3], w0);
        check(ok && fieldBytes < byteBytes,
            va("a grunt and its weapon stepping, by their fields: read back the same in %d bytes (byte by byte: %d)", fieldBytes,
                byteBytes));
        const int g2 = entityUpdate(u[2], (gb | U_ANGLE1) & ~U_LERPFINISH, 300, o1, a1, 13, 0, 0, 0);
        t.cur.clear();
        t.cur.add(300, u[2], g2);
        t.cur.add(301, u[3], w1);
        int layoutBytes = 0;
        ok = roundTrip(&t.base, t.cur, 4096, layoutBytes) && sameSnapshot(t.read, t.cur);
        check(ok && layoutBytes < g2 + w1, va("the grunt's fields changed (a pitch sent, no lerp): read back the same in %d bytes "
                                              "(whole: %d)", layoutBytes, g2 + w1 + 4));
        // not an update (a record of an entity's key that is no update: no U_SIGNAL) and a truncated one: byte by byte
        t.cur.bytes[t.cur.records[0].at] = 0x01;
        int oddBytes = 0;
        ok = roundTrip(&t.base, t.cur, 4096, oddBytes) && sameSnapshot(t.read, t.cur);
        check(ok, va("a record that is no update: read back the same (%d bytes)", oddBytes));
    }
    net_message = savedMsg;
    msg_readcount = savedCount;
    msg_badread = savedBad;
    tally.verdict("netsnap selftest");
}

void init()
{
    Cmd_AddCommand("vr_netsnap_selftest", selftest_f);
    // vr_net_bytes [reset]: what this client read from its server, by kind ("mp_netbytes:"; reset: from zero)
    Cmd_AddCommand("vr_net_bytes", [] {
        if(Cmd_Argc() > 1 && !q_strcasecmp(Cmd_Argv(1), "reset"))
        {
            bytesReset();
            poseEntries.clear();
            poseDists.clear();
        }
        bytesReport();
        if(Cmd_Argc() > 1 && !q_strcasecmp(Cmd_Argv(1), "poses"))
        {
            // (each armed humanoid's pose entries read since the reset, and its mean distance from us at them: the rate's
            // priorities, B12)
            for(za::SizeT e = 0; e < poseEntries.size(); e++)
            {
                if(poseEntries[e] > 0)
                {
                    Con_Printf("mp_netprio: ent=%d dist=%.0f entries=%d\n", static_cast<int>(e),
                        e < poseDists.size() ? poseDists[e] / static_cast<float>(poseEntries[e]) : 0.f, poseEntries[e]);
                }
            }
            Con_Printf("mp_netprio: done\n");
        }
    });
}

} // namespace qvr::netsnap

extern "C" void VR_NoteServerMessage(int kind)
{
    qvr::netsnap::noteMessage(kind);
}

extern "C" void VR_NoteServerCommand(int at)
{
    qvr::netsnap::noteCommand(at);
}

extern "C" void VR_InterpFrame()
{
    qvr::netsnap::interpFrame();
}

extern "C" int VR_InterpEntity(int num, float* origin, float* angles)
{
    return qvr::netsnap::interpEntity(num, origin, angles) ? 1 : 0;
}
