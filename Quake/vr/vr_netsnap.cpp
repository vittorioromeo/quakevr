// vr_netsnap.cpp -- snapshots down, acknowledged and delta-coded, and the entities' interpolation buffer (vr_netsnap.hpp;
// MP_PLAN.md, B7b).

#include "vr_netsnap.hpp"
#include "vr_seat.hpp"
#include "vr_clientprefs.hpp"
#include "vr_netmove.hpp"
#include "vr_mpavatar.hpp"
#include "vr_engine.hpp"
#include "vr_api.h"
#include "vr_cvars.hpp"
#include "vr_protocol.hpp"
#include "vr_progs.hpp"
#include "vr_testkit.hpp"
#include "vr_mem.hpp"
#include "vr_netragdoll.hpp"

#include "Zancle/Algorithm/LowerBound.hpp"
#include "Zancle/Algorithm/Sort.hpp"
#include "Zancle/Algorithm/StableSort.hpp"
#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Base/Memcmp.hpp"
#include "Zancle/Base/Memcpy.hpp"
#include "Zancle/Base/Memset.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Base/Strcmp.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/MinMax.hpp"


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
    za::quickSort(records.begin(), records.end(), [](const Record& a, const Record& b) { return a.key < b.key; });
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

// `s`'s record of `key` (records sorted by key), nullptr none.
[[nodiscard]] const Record* findKey(const Snapshot& s, int key)
{
    const Record* const first = s.records.data();
    const Record* const last = first + s.records.size();
    const Record* const r = za::lowerBound(first, last, key, [](const Record& x, int k) { return x.key < k; });
    return r != last && r->key == key ? r : nullptr;
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
constexpr int fieldOrder[wireFields] = {15, 1, 5, 7, 9, 8, 6, 10, 0, 2, 3, 4, 11, 12, 13, 14}; // mask bit 1 + i -> field
constexpr za::U32 maskTrend = 1u, maskTail = 1u << (wireFields + 1), maskBits = 1u << (wireFields + 2);
[[nodiscard]] constexpr za::U32 maskField(int m)
{
    return 1u << (1 + m);
}
[[nodiscard]] constexpr bool trendField(int f) // the origins and angles: a moving thing's
{
    return f >= 5 && f <= 10;
}
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
    ZA_MEMCPY(out + at, e.tail, static_cast<za::SizeT>(e.tailLen));
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

// seat-shared: scratch
za::U8 fieldsBuf[fieldsMost];   // (an encoding by fields: encode's)
// seat-shared: scratch
za::U8 fieldsCheck[fieldsMost]; // (... its decoding: fieldsVerify's, decode's)

// Each field's reference (`ref`): the base's value, or with `trend` (the base's older record, `older`; `ahead` frames past
// the base, it `span` past the older) an origin's or angle's where its last change carries it (the base's plus that
// change times ahead / span, in integers: the same on both sides). False: no trend there (the older not an update).
[[nodiscard]] bool fieldRefs(const EntFields& b, const Trend* trend, const za::U8* older, int olderLen, unsigned pflags,
    za::U32* ref)
{
    ZA_MEMCPY(ref, b.v, sizeof(b.v));
    if(!trend)
    {
        return true;
    }
    EntFields o;
    if(!older || trend->span <= 0 || !parseFields(older, olderLen, pflags, o))
    {
        return false;
    }
    for(int f = 0; f < wireFields; f++)
    {
        const za::U32 bit = static_cast<za::U32>(wireBit[f]);
        if(!trendField(f) || !(b.bits & bit) || !(o.bits & bit))
        {
            continue;
        }
        const int n = fieldSize(f, pflags);
        const za::U32 zig = fieldChange(o.v[f], b.v[f], n);
        const za::I64 d = (zig & 1u) ? -static_cast<za::I64>(zig >> 1) - 1 : static_cast<za::I64>(zig >> 1);
        const za::I64 step = d * trend->ahead / trend->span;
        const za::U32 mask = n >= 4 ? 0xFFFFFFFFu : (1u << (8 * n)) - 1u;
        ref[f] = (b.v[f] + static_cast<za::U32>(step)) & mask;
    }
    return true;
}

// `cur` (an entity's update) by its fields against `base`'s into `out` (at most fieldsMost): its size, -1 when not. With
// `trend` (and the base's older record), the origins and angles against their trend too when that is smaller.
[[nodiscard]] int fieldsEncode(const za::U8* base, int baseLen, const za::U8* cur, int curLen, unsigned pflags, za::U8* out,
    const Trend* trend = nullptr, const za::U8* older = nullptr, int olderLen = 0)
{
    EntFields b, c;
    if(curLen > maxDeltaLen || !parseFields(base, baseLen, pflags, b) || !parseFields(cur, curLen, pflags, c) || b.num != c.num)
    {
        return -1;
    }
    za::U32 mask = 0;
    za::U32 zig[wireFields]{};
    za::U32 refs[2][wireFields];
    const bool trended = trend && fieldRefs(b, trend, older, olderLen, pflags, refs[1]);
    (void)fieldRefs(b, nullptr, nullptr, 0, pflags, refs[0]);
    int bestCost = 1 << 30;
    for(int k = 0; k < (trended ? 2 : 1); k++)
    {
        za::U32 km = k ? maskTrend : 0u;
        za::U32 kz[wireFields]{};
        int cost = 0;
        for(int m = 0; m < wireFields; m++)
        {
            const int f = fieldOrder[m];
            if(!(c.bits & static_cast<za::U32>(wireBit[f])))
            {
                continue;
            }
            kz[m] = fieldChange(refs[k][f], c.v[f], fieldSize(f, pflags));
            km |= kz[m] != 0 ? maskField(m) : 0u;
            cost += kz[m] != 0 ? varintSize(kz[m]) : 0;
        }
        cost += varintSize(km);
        if(cost < bestCost)
        {
            bestCost = cost;
            mask = km;
            ZA_MEMCPY(zig, kz, sizeof(zig));
        }
    }
    const bool tailSame = b.tailLen == c.tailLen && ZA_MEMCMP(b.tail, c.tail, static_cast<za::SizeT>(c.tailLen)) == 0;
    mask |= tailSame ? 0u : maskTail;
    mask |= b.bits != c.bits ? maskBits : 0u;
    int at = putVarint(out, mask);
    if(mask & maskBits)
    {
        at += putVarint(out + at, b.bits ^ c.bits);
    }
    for(int m = 0; m < wireFields; m++)
    {
        if(mask & maskField(m))
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
            ZA_MEMCPY(out + at, tmp, static_cast<za::SizeT>(sb.cursize));
            at += sb.cursize;
        }
        else
        {
            if(at + c.tailLen > fieldsMost)
            {
                return -1;
            }
            ZA_MEMCPY(out + at, c.tail, static_cast<za::SizeT>(c.tailLen));
            at += c.tailLen;
        }
    }
    return at;
}

// fieldsEncode's (`d`, at most `len` bytes) against `base` into `out` (fieldsMost): the update's length, -1 when malformed;
// `used`: the bytes read.
[[nodiscard]] int fieldsDecode(const za::U8* base, int baseLen, const za::U8* d, int len, unsigned pflags, za::U8* out, int& used,
    const Trend* trend = nullptr, const za::U8* older = nullptr, int olderLen = 0)
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
    za::U32 ref[wireFields];
    if(!fieldRefs(e, (mask & maskTrend) ? trend : nullptr, older, olderLen, pflags, ref) || ((mask & maskTrend) && !trend))
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
    for(int m = 0; m < wireFields; m++)
    {
        const int f = fieldOrder[m];
        const bool present = (e.bits & static_cast<za::U32>(wireBit[f])) != 0;
        e.v[f] = present ? ref[f] : 0;
        if(!(mask & maskField(m)))
        {
            continue;
        }
        za::U32 zig = 0;
        if(!present || !getVarint(d, len, at, zig))
        {
            return -1;
        }
        e.v[f] = fieldApply(ref[f], zig, fieldSize(f, pflags));
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
            ZA_MEMCPY(tail, e.tail, static_cast<za::SizeT>(tl));
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
            ZA_MEMCPY(tail, d + at, static_cast<za::SizeT>(tl));
            at += tl;
        }
        e.tail = tail;
        e.tailLen = tl;
    }
    used = at;
    return writeFields(e, pflags, out, fieldsMost);
}

// fieldsEncode's `size` bytes in fieldsBuf decode against `base` to `cur` exactly (else it is not sent: B12b's safety).
[[nodiscard]] bool fieldsVerify(const za::U8* base, int baseLen, const za::U8* cur, int curLen, int size, unsigned pflags,
    const Trend* trend = nullptr, const za::U8* older = nullptr, int olderLen = 0)
{
    int used = 0;
    const int n = fieldsDecode(base, baseLen, fieldsBuf, size, pflags, fieldsCheck, used, trend, older, olderLen);
    return n == curLen && used == size && ZA_MEMCMP(fieldsCheck, cur, static_cast<za::SizeT>(n)) == 0;
}

} // namespace

namespace
{

// encode's first pass: each record that differs from the base's (in key order), its op and its size (without its head)
struct Entry
{
    za::I32 key{0}, op{0}, size{0}, ci{-1}; // ci: its record in `cur` (-1: gone)
    za::I32 kept{-1}; // opFields: its encoding's first byte in fieldsKept (the first pass's, written as it is: review B7)
    float prio{0.f};
    bool chosen{true};
    bool cosmetic{false}; // (B12c: a cosmetic fragment's: its own budget)
};
// seat-shared: the encode's scratch
za::Vector<Entry> entries;   // (encode's scratch)
// seat-shared: the encode's scratch
za::Vector<za::U8> fieldsKept; // (... the first pass's encodings by fields, verified: the write pass's bytes, review B7)
// seat-shared: the encode's scratch
za::Vector<za::I32> byPrio;  // (... the entries in priority order)
constexpr int headMost = 3;  // an entry's head: a key's step under 2^19
constexpr float nearUnits = 256.f; // a record's weight: 1 here, halving each this much farther ...
constexpr float aheadWeight = 2.f; // ... twice ahead of his view (within 60 degrees), twice again another player's
struct SeatNetSnapPoses // the seat's (seat::Part::NetSnapPoses; splitscreen phase 5)
{
    za::Vector<int> poseEntries; // the client: the pose entries read of each entity (vr_net_bytes poses; the rate's priorities)
    za::Vector<float> poseDists; // ... and the sum of its distances from us at each (their mean: where it was meanwhile)
    za::Vector<float> poseWeights; // ... and of its weights for us at each (weightOf's, from our view: near and ahead; B16)
    long long entOps[4][2]{};    // the client: the entities' entries read by op (gone, whole, delta, fields): count, bytes
};
SeatNetSnapPoses& seatSnapPoses()
{
    return qvr::seat::part<SeatNetSnapPoses>(qvr::seat::current(), qvr::seat::Part::NetSnapPoses);
}
// The server: its snapshots' entity entries written, by entity number (vr_net_classes: by classname; B12c's gib storms)
struct SentTally
{
    long long bytes{0};
    int entries{0}, whole{0};
};
// seat-shared: the server's encode
za::Vector<SentTally> sentByEnt;
// seat-shared: the server's encode
bool tallySent{false}; // (serverEnd's encode: not the self-test's)
// (B19) vr_net_posewaits: one client slot's pose records as they went, by entity: how many, the frames each had waited
// plus one summed, and its weight then summed (the rate's priorities as the server ran them: changed records only, so
// a pose that changes less often, an idle humanoid's, is not counted as starved; mp_armed)
struct PoseWait
{
    long long sent{0};
    double waitSum{0.0}, weightSum{0.0};
};
struct PoseWaits
{
    int slot{-1};
    za::Vector<PoseWait> byEnt;
    za::Vector<za::U8> written; // (serverEnd's encode: the pose records it wrote, by entity)
};
// seat-shared: the server's encode
PoseWaits poseWaits;
// seat-shared: the server's encode
int cosmeticWritten{0}; // (encode's last: its cosmetic fragments' entries' bytes)

} // namespace

int encode(const Snapshot* base, const Snapshot& cur, sizebuf_t* body, int budget, Snapshot& sent, unsigned pflags,
    const Trend* trend, const float* prio, za::U8* unsent, int soft, const za::U8* cosmetic, int cosmeticBudget,
    int cosmeticFill, int ragSoft)
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
    fieldsKept.clear();
    int need = body->cursize;
    int cosmeticNeed = 0;
    bool ragChanged = false; // (ragsync2: a ragdoll's changed record, under ragSoft)
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
            if(b->len == c->len && ZA_MEMCMP(base->data(*b), cur.data(*c), static_cast<za::SizeT>(c->len)) == 0)
            {
                continue;
            }
            const int wholeSize = varintSize(static_cast<za::U32>(c->len)) + c->len;
            const int deltaSize = b->len == c->len && c->len <= maxDeltaLen ? bytesDelta(nullptr, base->data(*b), cur.data(*c), c->len)
                                                                            : wholeSize + 1;
            // (an entity's update: by its fields too, when its decoding gives it back exactly; the smallest goes)
            const Record* o = trend && keyClass(c->key) == 1 ? findKey(*trend->older, c->key) : nullptr;
            const za::U8* od = o ? trend->older->data(*o) : nullptr;
            const int ol = o ? o->len : 0;
            int fieldsSize = keyClass(c->key) == 1 ? fieldsEncode(base->data(*b), b->len, cur.data(*c), c->len, pflags, fieldsBuf,
                                                         trend, od, ol)
                                                   : -1;
            fieldsSize = fieldsSize >= 0 && fieldsVerify(base->data(*b), b->len, cur.data(*c), c->len, fieldsSize, pflags, trend, od, ol)
                             ? fieldsSize
                             : wholeSize + 1;
            const int ci1 = static_cast<za::I32>(ci - 1);
            e = fieldsSize < deltaSize && fieldsSize < wholeSize ? Entry{c->key, opFields, fieldsSize, ci1}
                : deltaSize < wholeSize                          ? Entry{c->key, opDelta, deltaSize, ci1}
                                                                 : Entry{c->key, opWhole, wholeSize, ci1};
            if(e.op == opFields)
            {
                e.kept = static_cast<za::I32>(fieldsKept.size());
                fieldsKept.insert(fieldsKept.end(), fieldsBuf, fieldsBuf + fieldsSize);
            }
        }
        e.prio = e.ci < 0 ? INFINITY : prio ? prio[e.ci] : 0.f;
        e.cosmetic = cosmetic && e.ci >= 0 && cosmetic[e.ci] != 0;
        need += headMost + e.size;
        cosmeticNeed += e.cosmetic ? headMost + e.size : 0;
        ragChanged = ragChanged || (e.ci >= 0 && keyClass(e.key) == 5);
        entries.emplaceBack(e);
    }
    // (B12c: the cosmetic fragments' own budget, by priority; without priorities they go as the rest. B18: and only the
    // room the other entries leave under `cosmeticFill` bytes of body: a heavy fight's own updates leave a gib storm
    // less -- mp_armed's combat link 36.6 KB/s against its 35, a grunt gibbed in the window, 105 B of fragments a datagram)
    // (a quarter of their budget always: the fragments never all wait)
    if(cosmeticBudget >= 0 && cosmeticFill > 0)
    {
        cosmeticBudget = za::min(cosmeticBudget, za::max(cosmeticBudget / 4, cosmeticFill - (need - cosmeticNeed)));
    }
    const bool cosmeticOver = prio && cosmeticBudget >= 0 && cosmeticNeed > cosmeticBudget;
    ragSoft = ragSoft < 0 ? soft : za::min(ragSoft, soft);
    if(need > soft || cosmeticOver || (prio && ragChanged && need > ragSoft))
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
            // (ragsync2) A ragdoll's changed record goes only in the room every other entry leaves (after them all,
            // whatever its priority, and never past his bucket: the first-finite rule below is theirs): the ragdolls
            // under vr_net_ragdoll_bytes take from the ragdolls only (mp_ragsync: a dropped shotgun and backpacks 1.75-2.3
            // units p95 behind with the ragdolls ahead of them in a 1400-byte datagram, 0.04 without).
            const auto last = [](const Entry& e) { return e.prio != INFINITY && keyClass(e.key) == 5; };
            za::stableSort(byPrio.begin(), byPrio.end(), [&last](za::I32 a, za::I32 b) {
                const Entry& ea = entries[static_cast<za::SizeT>(a)];
                const Entry& eb = entries[static_cast<za::SizeT>(b)];
                return last(ea) != last(eb) ? !last(ea) : ea.prio > eb.prio;
            });
            int cosmeticUsed = 0;
            bool firstFinite = true;
            for(const za::I32 i : byPrio)
            {
                Entry& e = entries[static_cast<za::SizeT>(i)];
                const int cost = headMost + e.size;
                const bool cosmeticRoom = !e.cosmetic || !cosmeticOver || cosmeticUsed + cost <= cosmeticBudget;
                e.chosen = e.prio == INFINITY ? used + cost <= budget : used + cost <= (last(e) ? ragSoft : soft) && cosmeticRoom;
                firstFinite = firstFinite && !last(e);
                // (B12c) Under a rate (soft under the datagram's room), the highest priority goes even past what his
                // bucket holds now when it fits the datagram and the bucket is not in debt: the bucket goes below
                // zero and the next datagrams carry only what must go until it is paid back (his rate held on
                // average). Else a record larger than a frame's share of his rate never went while smaller, lower
                // ones took the room every frame (mp_armed: the two nearest armed humanoids' poses, 1 entry each in
                // 5 s, the farthest two's 150).
                if(e.prio != INFINITY && firstFinite)
                {
                    firstFinite = false;
                    e.chosen = e.chosen || (cosmeticRoom && soft > 0 && soft < budget && used + cost <= budget);
                }
                used += e.chosen ? cost : 0;
                cosmeticUsed += e.chosen && e.cosmetic ? cost : 0;
            }
        }
    }

    // Written in key order: the chosen entries (each as long as its real head fits too); the others keep the base's
    // record on both sides (a new one: none yet), `unsent` marks their records.
    int written = 0, prevKey = -1;
    cosmeticWritten = 0;
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
        const int entryAt = body->cursize;
        netmove::writeVarint(body, head);
        if(e.op == opDelta)
        {
            bytesDelta(body, base->data(*b), cur.data(*c), c->len);
        }
        else if(e.op == opFields)
        {
            SZ_Write(body, fieldsKept.data() + e.kept, e.size); // (the first pass's bytes, measured and checked)
        }
        else if(e.op == opWhole)
        {
            netmove::writeVarint(body, static_cast<za::U32>(c->len));
            SZ_Write(body, cur.data(*c), c->len);
        }
        if(c)
        {
            addFrom(sent, cur, *c);
            sent.records.back().fresh = true; // (his client takes a place from it: B13c's lag compensation)
        }
        cosmeticWritten += e.cosmetic ? body->cursize - entryAt : 0;
        if(tallySent && keyClass(e.key) == 1)
        {
            if(e.key >= static_cast<int>(sentByEnt.size()))
            {
                sentByEnt.resize(static_cast<za::SizeT>(e.key) + 1);
            }
            SentTally& t = sentByEnt[static_cast<za::SizeT>(e.key)];
            t.bytes += body->cursize - entryAt;
            t.entries++;
            t.whole += e.op == opWhole ? 1 : 0;
        }
        if(tallySent && keyClass(e.key) == 4 && e.key - poseKey >= 0 &&
            e.key - poseKey < static_cast<int>(poseWaits.written.size()))
        {
            poseWaits.written[static_cast<za::SizeT>(e.key - poseKey)] = 1;
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

bool decode(const Snapshot* base, int bodyLen, Snapshot& out, unsigned pflags, const Trend* trend, long long* tally)
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
                seatSnapPoses().entOps[tallyOp][0]++;
                seatSnapPoses().entOps[tallyOp][1] += entryAt - tallyAt;
            }
            tallyClass = keyClass(key);
            tallyOp = static_cast<int>(head & 3u);
            tallyAt = entryAt;
            const int e = key - poseKey;
            if(op != opGone && e >= 0 && e < 65536)
            {
                if(e >= static_cast<int>(seatSnapPoses().poseEntries.size()))
                {
                    seatSnapPoses().poseEntries.resize(static_cast<za::SizeT>(e) + 1, 0);
                    seatSnapPoses().poseDists.resize(static_cast<za::SizeT>(e) + 1, 0.f);
                    seatSnapPoses().poseWeights.resize(static_cast<za::SizeT>(e) + 1, 0.f);
                }
                seatSnapPoses().poseEntries[static_cast<za::SizeT>(e)]++;
                if(e < cl.num_entities && cl.viewentity > 0 && cl.viewentity < cl.num_entities)
                {
                    const float* o = cl_entities[e].origin;
                    const float* me = cl_entities[cl.viewentity].origin;
                    const float d[3]{o[0] - me[0], o[1] - me[1], o[2] - me[2]};
                    const float dist = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
                    seatSnapPoses().poseDists[static_cast<za::SizeT>(e)] += dist;
                    vec3_t fwd, right, up;
                    AngleVectors(cl.viewangles, fwd, right, up);
                    const float dot = d[0] * fwd[0] + d[1] * fwd[1] + d[2] * fwd[2];
                    seatSnapPoses().poseWeights[static_cast<za::SizeT>(e)] +=
                        nearUnits / (nearUnits + dist) * (dot > 0.5f * dist ? aheadWeight : 1.f);
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
            out.records.back().fresh = true;
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
            const Record* o = trend ? findKey(*trend->older, key) : nullptr;
            const int n = fieldsDecode(base->data(*b), b->len, net_message.data + msg_readcount, end - msg_readcount, pflags,
                fieldsCheck, used, trend, o ? trend->older->data(*o) : nullptr, o ? o->len : 0);
            if(n < 0)
            {
                return false;
            }
            out.add(key, fieldsCheck, n);
            out.records.back().fresh = true;
            msg_readcount += used;
            continue;
        }
        const za::I32 at = static_cast<za::I32>(out.bytes.size());
        addFrom(out, *base, *b);
        out.records.back().fresh = true;
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
            seatSnapPoses().entOps[tallyOp][0]++;
            seatSnapPoses().entOps[tallyOp][1] += msg_readcount - tallyAt;
        }
    }
    return msg_readcount == end && !msg_badread;
}

namespace
{

// ---- Server --------------------------------------------------------------------------------------------------------

// Per client slot: the snapshots sent (`ring` back, by sequence number), the newest it acknowledged, the counts.
constexpr int sentLogLen = 80; // (B13c) snapshots logged for the lag compensation: 1.1 s at 72 Hz, as it keeps

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
    // (B13c) The lag compensation's log of what he was sent, longer than the ring (his shots look back up to 1 s): each
    // snapshot's time (its svc_time) and the targets' records in it (players' and monsters' entities, avatars), each
    // whether it was new or changed there (his client took a place from it)
    struct Sent
    {
        double time{-1.0};
        za::Vector<za::I32> keys;
        za::Vector<za::U8> fresh;
    };
    Sent sentLog[sentLogLen];
    int sentHead{0}; // the next to write
    double nextSendAt{-1.0}; // (B13c, vr_net_sendhz) his next datagram's server time
};
// seat-shared: the server's, each player slot's
za::Vector<ServerSlot> serverSlots;

// Snapshot `sent` (just encoded) into his log: its targets' records and which were written.
void logSent(ServerSlot& s, const Snapshot& sent)
{
    ServerSlot::Sent& l = s.sentLog[s.sentHead];
    s.sentHead = (s.sentHead + 1) % sentLogLen;
    l.time = qcvm->time;
    l.keys.clear();
    l.fresh.clear();
    for(const Record& r : sent.records)
    {
        const int klass = keyClass(r.key);
        const int e = klass == 1 ? r.key : klass == 2 ? r.key - avatarKey : -1;
        if(e <= 0 || e >= qcvm->num_edicts || (klass == 1 && !(static_cast<int>(EDICT_NUM(e)->v.flags) & (FL_MONSTER | FL_CLIENT))))
        {
            continue;
        }
        l.keys.pushBack(r.key);
        l.fresh.pushBack(r.fresh ? 1 : 0);
    }
}

constexpr double rateBurst = 0.1;  // s of his rate the bucket holds (at least a full datagram)
constexpr int netHeader = 8;       // NetQuake's datagram header (the bytes his link carries a datagram besides ours)

// A record's index in his waits (-1: his own state, never waiting): entities, avatars, poses by entity.
[[nodiscard]] int waitIndex(int key)
{
    const int klass = keyClass(key);
    const int ents = qcvm ? qcvm->max_edicts : 0;
    const int e = klass == 1 ? key : klass == 2 ? key - avatarKey : klass == 4 ? key - poseKey : klass == 5 ? key - ragKey : -1;
    if(e < 0 || e >= ents)
    {
        return -1;
    }
    return (klass == 1 ? 0 : klass == 2 ? 1 : klass == 4 ? 2 : 3) * ents + e;
}
constexpr int waitKinds = 4; // (waitIndex's: entities, avatars, poses, ragdolls)

// A record's weight for the player at `eye` looking along `fwd`: nearer and ahead first.
[[nodiscard]] float weightOf(int key, const float eye[3], const float fwd[3])
{
    const int klass = keyClass(key);
    const int e = klass == 1 ? key : klass == 2 ? key - avatarKey : klass == 5 ? key - ragKey : key - poseKey;
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
    w *= klass == 2 ? aheadWeight : 1.f;
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
    za::Vector<za::U8> cosmetic; // each record a cosmetic fragment's (B12c)
    za::Vector<za::U8> held;     // each ragdoll's record held as the base's (ragsync: holdRagdolls)
    za::Vector<za::SizeT> ragOrder; // (holdRagdolls: the changed ones, by priority)
    int chargeSlot{-1};        // the slot whose datagram serverCharge charges (serverEnd's)
    double most{0.0};          // (his bucket's size: serverBegin's)
};
// seat-shared: the server's encode
Building building;

// (ragsync) The ragdolls' changed records under vr_net_ragdoll_bytes a snapshot: the highest priorities (near, ahead,
// waited longest: bl.prio) go, the rest are held as the base's (unchanged: not sent, both sides keep the base's, his
// client draws between the poses it has) and wait, their priority growing. A new one (not in the base) always goes, and
// the first changed one whatever its size. A settled ragdoll costs nothing: its record is unchanged (its pose's time
// too: ragdoll::serverPose). -> the records held.
int holdRagdolls(const Snapshot* base)
{
    Building& bl = building;
    const za::SizeT n = bl.cur.records.size();
    bl.held.clear();
    bl.held.resize(n, 0);
    const int cap = static_cast<int>(vr_net_ragdoll_bytes.value);
    if(cap <= 0 || !base)
    {
        return 0;
    }
    za::Vector<za::SizeT>& order = bl.ragOrder;
    order.clear();
    int used = 0;
    for(za::SizeT i = 0; i < n; i++)
    {
        const Record& r = bl.cur.records[i];
        if(keyClass(r.key) != 5)
        {
            continue;
        }
        const Record* b = findKey(*base, r.key);
        if(!b)
        {
            used += r.len;
            continue;
        }
        if(b->len == r.len && ZA_MEMCMP(bl.cur.data(r), base->data(*b), static_cast<za::SizeT>(r.len)) == 0)
        {
            continue;
        }
        // (by priority, the highest first: an insertion, a few ragdolls)
        za::SizeT at = order.size();
        order.pushBack(i);
        while(at > 0 && bl.prio[order[at - 1]] < bl.prio[i])
        {
            order[at] = order[at - 1];
            at--;
        }
        order[at] = i;
    }
    int held = 0;
    for(const za::SizeT i : order)
    {
        Record& r = bl.cur.records[i];
        if(used == 0 || used + r.len <= cap)
        {
            used += r.len;
            continue;
        }
        const Record* b = findKey(*base, r.key);
        r.at = static_cast<za::I32>(bl.cur.bytes.size());
        r.len = b->len;
        if(b->len > 0)
        {
            bl.cur.bytes.emplaceBackRange(base->data(*b), static_cast<za::SizeT>(b->len));
        }
        bl.held[i] = 1;
        held++;
    }
    return held;
}

// (B12c) A cosmetic fragment's entity: a small gib or an explosion's chunk (nothing touches them: vr_smallgibs.qc,
// vr_explosiondebris.cpp), whose updates have a budget of their own (vr_net_cosmetic_bytes).
// Each entity's answer, once a server frame for every client's snapshot (review B7): by entity number, 0 not asked yet
// this frame, 1 no, 2 yes.
// seat-shared: the server's, once a server frame
za::Vector<za::U8> cosmeticByEnt;
// seat-shared: the server's
int cosmeticFrame{-1}; // (host_framecount when cosmeticByEnt was cleared)

[[nodiscard]] bool isCosmetic(int num)
{
    if(num <= 0 || num >= qcvm->num_edicts)
    {
        return false;
    }
    if(cosmeticFrame != host_framecount || static_cast<int>(cosmeticByEnt.size()) != qcvm->max_edicts)
    {
        cosmeticFrame = host_framecount;
        cosmeticByEnt.clear();
        cosmeticByEnt.resize(static_cast<za::SizeT>(qcvm->max_edicts), 0);
    }
    za::U8& known = cosmeticByEnt[static_cast<za::SizeT>(num)];
    if(known == 0)
    {
        const edict_t* ed = EDICT_NUM(num);
        const char* name = ed->free ? "" : PR_GetString(ed->v.classname);
        known = ZA_STRCMP(name, "vr_smallgib") == 0 || ZA_STRCMP(name, "vr_explosion_debris") == 0 ? 2 : 1;
    }
    return known == 2;
}

// (B13c, vr_net_test_hold n, a test aid) The other players' entities and avatars wait n snapshots in each n + 1: their
// records as the base's (unchanged: not sent, both sides keep the base's), as his rate's waits leave them.
void holdPlayers(const Snapshot* base, za::U32 seq)
{
    Building& bl = building;
    const int hold = static_cast<int>(vr_net_test_hold.value);
    if(hold <= 0 || !base || seq % static_cast<za::U32>(hold + 1) == 0)
    {
        return;
    }
    const int own = bl.client && bl.client->edict ? NUM_FOR_EDICT(bl.client->edict) : 0;
    for(Record& r : bl.cur.records)
    {
        const int klass = keyClass(r.key);
        const int e = klass == 1 ? r.key : klass == 2 ? r.key - avatarKey : -1;
        const Record* b = e >= 1 && e <= svs.maxclients && e != own ? findKey(*base, r.key) : nullptr;
        if(b)
        {
            r.at = static_cast<za::I32>(bl.cur.bytes.size());
            r.len = b->len;
            if(b->len > 0)
            {
                bl.cur.bytes.emplaceBackRange(base->data(*b), static_cast<za::SizeT>(b->len));
            }
        }
    }
}

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

bool serverSendsNow(client_t* client)
{
    const double hz = vr_net_sendhz.value;
    constexpr double frame = 1.0 / 72.0;
    if(hz <= 0.0 || hz >= 72.0 - 0.5 || vr_net_snapshots.value == 0.f || !client || !client->netconnection ||
        SV_IsLocalClient(client))
    {
        return true;
    }
    const int slot = static_cast<int>(client - svs.clients);
    ServerSlot* s = netmove::serverSends(slot) ? slotOf(slot, true) : nullptr;
    if(!s)
    {
        return true; // (NetQuake's datagrams: each frame, as before)
    }
    const double now = qcvm->time, period = 1.0 / za::max(hz, 1.0);
    if(s->nextSendAt >= 0.0 && now + 0.5 * frame < s->nextSendAt && s->nextSendAt - now <= 2.0 * period)
    {
        return false;
    }
    // (on the beat; a late frame, a new map's clock or a first: from now)
    const bool beat = s->nextSendAt >= 0.0 && now - s->nextSendAt < period && s->nextSendAt - now <= 2.0 * period;
    s->nextSendAt = beat ? s->nextSendAt + period : now + period;
    return true;
}

int clientRate(const client_t* client)
{
    const int own = client->edict ? za::max(0, static_cast<int>(clientprefs::value(clientprefs::indexOf(&vr_net_rate), &vr_net_rate, client->edict)))
                                  : 0;
    const int most = za::max(0, static_cast<int>(vr_net_rate_max.value));
    return own <= 0 ? most : most <= 0 ? own : za::min(own, most);
}

bool serverBegin(client_t* client)
{
    Building& b = building;
    b.on = false;
    if(vr_net_snapshots.value == 0.f || !client || !client->netconnection ||
        SV_IsLocalClient(client))
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
    // (his rate: his own preference under the server's ceiling; the bucket filled by the game's time since his last datagram)
    b.rate = clientRate(client);
    ServerSlot& s = *slotOf(slot, true);
    const double now = qcvm->time;
    if(b.rate > 0)
    {
        const double most = za::max(static_cast<double>(b.rate) * rateBurst, static_cast<double>(DATAGRAM_MTU + netHeader));
        s.tokens = s.filledAt < 0.0 || now < s.filledAt ? most : za::min(most, s.tokens + b.rate * (now - s.filledAt));
        b.most = most;
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

int serverPlayer()
{
    return building.on && building.client && building.client->edict ? NUM_FOR_EDICT(building.client->edict) : 0;
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
    holdPlayers(base, seq);
    Snapshot& sent = s->snaps[seq % ring];
    sizebuf_t body{};
    body.data = bl.body;
    body.maxsize = static_cast<int>(sizeof(bl.body));
    const za::SizeT before = bl.cur.records.size();

    // Its budget (B12): the room left after what is written and the broadcast to come (its events before poses, as when
    // the poses came after it; at most half), and under his rate what his bucket holds after them.
    const int broadcast = za::min(sv.datagram.cursize + VR_HeldBroadcastBytes(bl.slot), room / 2); // (B13c: and held)
    int budget = room - broadcast;
    if(bl.rate > 0)
    {
        budget = za::min(budget, static_cast<int>(s->tokens) - msg->cursize - broadcast - netHeader - 6);
    }
    budget = za::max(budget, 0);
    // (ragsync2) The ragdolls' room: after every other record, only what his bucket would spill by his next datagram
    // (what it cannot hold anyway): the others never wait behind the ragdolls, in this datagram or the ones after it.
    int ragSoft = budget;
    if(bl.rate > 0)
    {
        const double hz = vr_net_sendhz.value;
        const double period = hz > 0.0 && hz < 72.0 - 0.5 ? 1.0 / za::max(hz, 1.0) : 1.0 / 72.0;
        const int spill = static_cast<int>(s->tokens + bl.rate * period - bl.most);
        ragSoft = za::max(za::min(budget, spill - msg->cursize - broadcast - netHeader - 6), 0);
    }
    // Each record's priority: his own state always first; the others by weight (near, ahead, a player) times their wait.
    const za::SizeT n = bl.cur.records.size();
    bl.prio.resize(n);
    bl.unsent.clear();
    bl.unsent.resize(n, 0);
    const int ents = qcvm->max_edicts;
    if(static_cast<int>(s->waits.size()) != waitKinds * ents)
    {
        s->waits.clear();
        s->waits.resize(static_cast<za::SizeT>(waitKinds * ents), 0);
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
    bl.cosmetic.clear();
    bl.cosmetic.resize(n, 0);
    const int cosmeticBudget = static_cast<int>(vr_net_cosmetic_bytes.value);
    for(za::SizeT i = 0; i < n; i++)
    {
        const int key = bl.cur.records[i].key;
        bl.cosmetic[i] = cosmeticBudget > 0 && keyClass(key) == 1 && isCosmetic(key) ? 1 : 0;
        const int wi = waitIndex(key);
        bl.prio[i] = keyClass(key) == 0 || keyClass(key) == 3 ? INFINITY
                                                              : weightOf(key, eye, fwd) * (1.f + (wi >= 0 ? s->waits[static_cast<za::SizeT>(wi)] : 0));
    }
    s->counts.ragHeld += holdRagdolls(base);
    // (B12b: the base's own base, kept by both sides: the moving things' trend)
    Trend trend;
    const za::U32 older = base ? base->baseSeq : 0u;
    if(older != 0 && seq - older < static_cast<za::U32>(ring) && s->snaps[older % ring].seq == older)
    {
        trend = Trend{&s->snaps[older % ring], static_cast<int>(seq - base->seq), static_cast<int>(base->seq - older)};
    }
    const bool poseTally = bl.slot == poseWaits.slot;
    poseWaits.written.clear();
    poseWaits.written.resize(poseTally ? static_cast<za::SizeT>(ents) : 0, 0);
    tallySent = true;
    encode(base, bl.cur, &body, room, sent, sv.protocolflags, trend.older ? &trend : nullptr, bl.prio.data(), bl.unsent.data(),
        za::min(budget, room), bl.cosmetic.data(), cosmeticBudget > 0 ? cosmeticBudget : -1,
        static_cast<int>(vr_net_cosmetic_fill.value), ragSoft);
    tallySent = false;
    for(za::SizeT i = 0; i < n; i++)
    {
        bl.unsent[i] = bl.unsent[i] || bl.held[i] ? 1 : 0; // (a ragdoll held: it waited, holdRagdolls)
    }
    int cosmeticWaited = 0;
    for(za::SizeT i = 0; i < n; i++)
    {
        cosmeticWaited += bl.unsent[i] && bl.cosmetic[i] ? 1 : 0;
        s->counts.ragWaited += bl.unsent[i] && keyClass(bl.cur.records[i].key) == 5 ? 1 : 0;
    }
    sent.baseSeq = base ? base->seq : 0u;
    int waited = 0;
    for(za::SizeT i = 0; i < n; i++)
    {
        const int wi = waitIndex(bl.cur.records[i].key);
        if(wi >= 0)
        {
            za::U16& w = s->waits[static_cast<za::SizeT>(wi)];
            const int pe = bl.cur.records[i].key - poseKey;
            if(poseTally && keyClass(bl.cur.records[i].key) == 4 && pe >= 0 && pe < ents &&
                poseWaits.written[static_cast<za::SizeT>(pe)])
            {
                if(static_cast<int>(poseWaits.byEnt.size()) < ents)
                {
                    poseWaits.byEnt.resize(static_cast<za::SizeT>(ents));
                }
                PoseWait& t = poseWaits.byEnt[static_cast<za::SizeT>(pe)];
                t.sent++;
                t.waitSum += 1.0 + w;
                t.weightSum += static_cast<double>(bl.prio[i]) / (1.0 + w);
            }
            w = bl.unsent[i] ? static_cast<za::U16>(za::min(static_cast<int>(w) + 1, 60000)) : za::U16{0};
        }
        waited += bl.unsent[i];
    }
    bl.chargeSlot = bl.slot;
    sent.seq = seq;
    logSent(*s, sent);
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
    c.cosmeticBytes += cosmeticWritten;
    c.cosmeticWaited += cosmeticWaited;
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
        for(ServerSlot::Sent& l : s->sentLog)
        {
            l.time = -1.0;
        }
        s->nextSendAt = -1.0;
        if(!keepCounts)
        {
            s->counts = {};
        }
    }
    if(slotNum >= 0 && slotNum < MAX_SCOREBOARD)
    {
        VR_ClearHeldBroadcast(slotNum);
    }
}

int serverSentTimes(int slotNum, int key, double upTo, double* out, int most, double& oldest, bool& sightBegan)
{
    oldest = -1.0;
    sightBegan = false;
    const ServerSlot* s = slotOf(slotNum, false);
    if(!s)
    {
        return 0;
    }
    int n = 0;
    for(int k = 1; k <= sentLogLen && n < most; k++)
    {
        const ServerSlot::Sent& l = s->sentLog[(s->sentHead - k + 2 * sentLogLen) % sentLogLen];
        if(l.time < 0.0)
        {
            break; // (none yet, or forgotten by a reset)
        }
        if(l.time > upTo)
        {
            continue; // (made after the message he had read: not drawn yet)
        }
        oldest = l.time;
        const za::I32* const first = l.keys.data();
        const za::I32* const last = first + l.keys.size();
        const za::I32* const at = za::lowerBound(first, last, static_cast<za::I32>(key));
        if(at == last || *at != key)
        {
            sightBegan = true;
            break; // (out of his sight then: his buffer's history of it starts after this one)
        }
        if(l.fresh[static_cast<za::SizeT>(at - first)])
        {
            out[n++] = l.time;
        }
    }
    return n;
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
struct SeatNetSnap // the seat's (seat::Part::NetSnap; splitscreen phase 5)
{
    ClientState clientState;
};
SeatNetSnap& seatNetSnap()
{
    return qvr::seat::part<SeatNetSnap>(qvr::seat::current(), qvr::seat::Part::NetSnap);
}

// What this client read, by kind (noteMessage, noteCommand; B12's byte table).
struct ByteCounts
{
    long long reliable{0}, unreliable{0}, messages{0};
    long long svc[256]{};       // the unreliable messages' bytes by command (an entity's update: U_SIGNAL's)
    long long qvr[256]{};       // ... svc_quakevr's by its sub-command
    long long snap[keyClasses]{}; // a snapshot's entries by their keys' class (its own header: the rest of qvr's)
    long long relSvc[256]{};    // (mpoverflow) the reliable messages' bytes by command ...
    long long relQvr[256]{};    // ... svc_quakevr's by its sub-command ("mp_netbytes_rel:")
    bool reliableMsg{false};
    int openAt{-1}, openCmd{0}, openSub{0}; // the command being read: where it began
};
struct SeatNetSnapBytes // the seat's (seat::Part::NetSnapBytes; splitscreen phase 5)
{
    ByteCounts byteCounts;
};
SeatNetSnapBytes& seatSnapBytes()
{
    return qvr::seat::part<SeatNetSnapBytes>(qvr::seat::current(), qvr::seat::Part::NetSnapBytes);
}

// ---- The interpolation buffer ----------------------------------------------------------------------------------------

constexpr int historyLen = 40;          // messages kept per entity (raginterp: past the clock's furthest lag, below)
constexpr double tick = 1.0 / 72.0;     // the server's frame
constexpr double delayMin = 1.5 * tick; // the delay's least ...
constexpr double delayMost = 0.25;      // ... and most
// (raginterp) The history reaches back past the clock at its furthest: the longest delay and the most it may lag its
// target before it snaps (catching up after a hitch), a rest sample and the cushion included. With 8 (0.11 s) a delay
// over 7 ticks (the ragdolls' bigger snapshots' arrivals) put the clock before an entity's oldest message, drawn there:
// a thrown crate stepped 1-2 ticks ahead, not interpolated. (The check: after restGap.)
constexpr double delayMargin = 0.5 * tick;
constexpr double peakDecay = 0.01;      // s a second: the longest gap's hold on the delay fades this fast
constexpr double clockGain = 4.0;       // the delay's clock: this much faster a second behind (slower ahead) ...
constexpr double rateLeast = 0.85, rateMost = 1.15; // ... within these
constexpr double snapAfter = 0.25;      // s off its target: set there (a new map, a long stall)
constexpr double cushion = 0.5 * tick;  // the draw time this near the newest message: its clock slows, to a stop at it
constexpr double restGap = 4.0 * tick;  // (B12c) a thing unchanged longer than this rested: it sets off from its place
static_assert(historyLen * tick >= delayMost + snapAfter + 3.0 * tick, "the entity history must outlast the clock's lag");
static_assert(interpReach >= delayMost + snapAfter + 3.0 * tick, "interpReach (vr_netsnap.hpp) is the clock's furthest lag");

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
    // (mpopen) How it was last drawn (InterpWay) and why its history last started again (InterpRestart): vr_mp_bodylog's.
    mutable int way{0};
    int restarts{0};
    int restartWhy{0};
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
struct SeatNetSnapInterp // the seat's (seat::Part::NetSnapInterp; splitscreen phase 5)
{
    Interp interp;
};
SeatNetSnapInterp& seatSnapInterp()
{
    return qvr::seat::part<SeatNetSnapInterp>(qvr::seat::current(), qvr::seat::Part::NetSnapInterp);
}

void interpArrive(double t)
{
    Interp& ip = seatSnapInterp().interp;
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
void interpNote(int e, za::U32 prevSeq, za::U32 seq, bool fresh)
{
    Interp& ip = seatSnapInterp().interp;
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
        if(h.n > 0)
        {
            h.restarts++;
            h.restartWhy = ent.model != h.model ? 3 : prevSeq == 0 ? 2 : 1;
        }
        h.n = 0; // (new, or back in sight, or another thing in its slot: its history starts again)
    }
    h.model = ent.model;
    h.seq = seq;
    // (B12c) An unchanged record adds no place: its newest stands (a thing at rest, or one whose update waited: a cosmetic
    // fragment's budget, a rate's: drawn between its places as they come, not stopped a frame). One unchanged longer
    // than restGap rested there: that place again a frame before this one, so it sets off from it.
    if(!fresh && h.n > 0)
    {
        return;
    }
    if(h.n > 0)
    {
        const Sample last = h.s[(h.head - 1 + historyLen) % historyLen];
        if(cl.mtime[0] - last.t > restGap + 0.5 * tick)
        {
            Sample& rest = h.s[h.head];
            rest = last;
            rest.t = cl.mtime[0] - tick;
            h.head = (h.head + 1) % historyLen;
            h.n = za::min(h.n + 1, historyLen);
        }
    }
    Sample& s = h.s[h.head];
    s.t = cl.mtime[0];
    ZA_MEMCPY(s.origin, ent.msg_origins[0], sizeof(s.origin));
    ZA_MEMCPY(s.angles, ent.msg_angles[0], sizeof(s.angles));
    h.head = (h.head + 1) % historyLen;
    h.n = za::min(h.n + 1, historyLen);
}

// The snapshot's records parsed as the server's messages they are (net_message pointed at each); its entities' places
// into their histories when it is a new one (`prevSeq`: the one read before it; else 0 and seq 0).
void apply(const Snapshot& s, za::U32 prevSeq, za::U32 seq)
{
    netrag::clientSnapshotBegin();
    ClientCounts& counts = seatNetSnap().clientState.counts;
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
    netrag::clientSnapshotEnd(); // (the ragdolls not in it: gone, drawn animated again)
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
            interpNote(r.key, prevSeq, seq, r.fresh);
        }
    }
}

} // namespace

za::U32 clientAck()
{
    return seatNetSnap().clientState.wantsWhole ? 0u : seatNetSnap().clientState.last;
}

void clientWantWhole()
{
    seatNetSnap().clientState.wantsWhole = seatNetSnap().clientState.last != 0;
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
    ClientState& c = seatNetSnap().clientState;
    const int bodyStart = msg_readcount;
    bool ok = seq > c.last && dist < static_cast<za::U32>(ring);
    const Snapshot* base = nullptr;
    if(ok && dist != 0)
    {
        base = &c.snaps[(seq - dist) % ring];
        ok = seq > dist && base->seq == seq - dist;
    }
    // (B12b: the base's own base, the moving things' trend: the server's choice, by the same rule)
    Trend trend;
    const za::U32 older = base ? base->baseSeq : 0u;
    if(older != 0 && seq - older < static_cast<za::U32>(ring) && c.snaps[older % ring].seq == older)
    {
        trend = Trend{&c.snaps[older % ring], static_cast<int>(seq - base->seq), static_cast<int>(base->seq - older)};
    }
    Snapshot& out = c.snaps[seq % ring];
    ok = ok && decode(base, static_cast<int>(len), out, cl.protocolflags, trend.older ? &trend : nullptr, seatSnapBytes().byteCounts.snap);
    out.baseSeq = ok && base ? base->seq : 0u;
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
    netrag::clientClear();
    ClientState& c = seatNetSnap().clientState;
    c.last = 0;
    for(Snapshot& s : c.snaps)
    {
        s.clear();
    }
    seatSnapInterp().interp.valid = false;
    for(History& h : seatSnapInterp().interp.of)
    {
        h.n = 0;
        h.model = nullptr;
    }
}

namespace
{
// Forgotten when the client's state is cleared (vr_mem.hpp's resets): no snapshot read: the server's next whole, B7b; the interpolation buffer empty.
// seat-shared: the reset hook (it clears the current seat's)
mem::Reset clientStateReset{"netsnap", mem::ResetEvent::ClientState, mem::ResetPhase::State, &clientClear};
} // namespace

ClientCounts clientCounts()
{
    return seatNetSnap().clientState.counts;
}

void noteMessage(int kind)
{
    ByteCounts& b = seatSnapBytes().byteCounts;
    b.reliableMsg = kind == 1;
    b.openAt = -1;
    b.messages++;
    (b.reliableMsg ? b.reliable : b.unreliable) += net_message.cursize;
}

void noteCommand(int at)
{
    ByteCounts& b = seatSnapBytes().byteCounts;
    if(b.openAt >= 0 && at > b.openAt && !b.reliableMsg)
    {
        (b.openCmd == protocol::svc_quakevr ? b.qvr[b.openSub] : b.svc[b.openCmd]) += at - b.openAt;
    }
    else if(b.openAt >= 0 && at > b.openAt)
    {
        (b.openCmd == protocol::svc_quakevr ? b.relQvr[b.openSub] : b.relSvc[b.openCmd]) += at - b.openAt;
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
    seatSnapBytes().byteCounts = ByteCounts{};
    ZA_MEMSET(seatSnapPoses().entOps, 0, sizeof(seatSnapPoses().entOps));
}

// "mp_netbytes:" the bytes read since the connection (or `vr_net_bytes reset`): reliable and unreliable, the unreliable
// by kind (the snapshot's entries by class: its player state, entities, avatars, walk state, poses; its header; the
// poses, avatars and entities outside it; sounds, temp entities, the rest), then the three biggest of the rest, then the
// snapshot's entity entries by op (ent_ops: gone, whole, byte by byte, by fields; each count/bytes).
void bytesReport()
{
    const ByteCounts& b = seatSnapBytes().byteCounts;
    long long snapEntries = 0;
    for(const long long n : b.snap)
    {
        snapEntries += n;
    }
    const long long snap = b.qvr[protocol::QVR_SVC_SNAPSHOT], pose = b.qvr[protocol::QVR_SVC_ARMPOSE],
                    avatar = b.qvr[protocol::QVR_SVC_AVATAR], ent = b.svc[U_SIGNAL], cd = b.svc[svc_clientdata],
                    sound = b.svc[svc_sound], tent = b.svc[svc_temp_entity], voice = b.qvr[protocol::QVR_SVC_VOICE];
    const long long rest = b.unreliable - snap - pose - avatar - ent - cd - sound - tent - voice;
    // (the rest's three biggest: "q<sub>" a svc_quakevr's, else the command's number)
    int top[3] = {-1, -1, -1};
    long long topN[3] = {0, 0, 0};
    for(int i = 0; i < 512; i++)
    {
        const bool q = i >= 256;
        const int c = i & 255;
        if((q && (c == protocol::QVR_SVC_SNAPSHOT || c == protocol::QVR_SVC_ARMPOSE || c == protocol::QVR_SVC_AVATAR ||
                     c == protocol::QVR_SVC_VOICE)) ||
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
               "snap_pred=%lld snap_pose=%lld snap_rag=%lld snap_hdr=%lld pose=%lld avatar=%lld ent=%lld cd=%lld sound=%lld tent=%lld "
               "voice=%lld rest=%lld top=%s%d:%lld,%s%d:%lld,%s%d:%lld ent_ops=%lld/%lld,%lld/%lld,%lld/%lld,%lld/%lld\n",
        b.messages, b.reliable, b.unreliable, snap, b.snap[0], b.snap[1], b.snap[2], b.snap[3], b.snap[4], b.snap[5],
        snap - snapEntries, pose, avatar, ent, cd, sound, tent, voice, rest, top[0] >= 256 ? "q" : "", top[0] & 255, topN[0],
        top[1] >= 256 ? "q" : "", top[1] & 255, topN[1], top[2] >= 256 ? "q" : "", top[2] & 255, topN[2], seatSnapPoses().entOps[0][0], seatSnapPoses().entOps[0][1],
        seatSnapPoses().entOps[1][0], seatSnapPoses().entOps[1][1], seatSnapPoses().entOps[2][0], seatSnapPoses().entOps[2][1], seatSnapPoses().entOps[3][0], seatSnapPoses().entOps[3][1]);
    // (mpoverflow) The reliable stream's six biggest kinds ("q<sub>": a svc_quakevr's, else the command's number)
    constexpr int relTop = 6;
    int rt[relTop];
    long long rn[relTop];
    for(int k = 0; k < relTop; k++)
    {
        rt[k] = -1;
        rn[k] = 0;
    }
    for(int i = 0; i < 512; i++)
    {
        const long long n = i >= 256 ? b.relQvr[i & 255] : b.relSvc[i];
        for(int k = 0; k < relTop; k++)
        {
            if(n > rn[k])
            {
                for(int m = relTop - 1; m > k; m--)
                {
                    rt[m] = rt[m - 1];
                    rn[m] = rn[m - 1];
                }
                rt[k] = i;
                rn[k] = n;
                break;
            }
        }
    }
    Con_Printf("mp_netbytes_rel: reliable=%lld top=", b.reliable);
    for(int k = 0; k < relTop && rt[k] >= 0; k++)
    {
        Con_Printf("%s%s%d:%lld", k ? "," : "", rt[k] >= 256 ? "q" : "", rt[k] & 255, rn[k]);
    }
    Con_Printf("\n");
}

bool interpActive()
{
    return seatSnapInterp().interp.valid && vr_net_interp.value != 0.f && seatNetSnap().clientState.last != 0 && cls.state == ca_connected &&
           !cls.demoplayback;
}

void interpFrame()
{
    Interp& ip = seatSnapInterp().interp;
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
    const Interp& ip = seatSnapInterp().interp;
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
        h.way = 4;
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
    h.way = !older ? 1 : !newer ? 2 : 0;
    if(!only)
    {
        const float cut = teleportCut(newer->t - older->t);
        for(int j = 0; j < 3; j++)
        {
            if(za::abs(newer->origin[j] - older->origin[j]) > cut)
            {
                only = older; // (a teleport: no lerp across it; the newer when its time comes)
                h.way = 3;
            }
        }
    }
    if(only)
    {
        ZA_MEMCPY(origin, only->origin, sizeof(only->origin));
        ZA_MEMCPY(angles, only->angles, sizeof(only->angles));
        return true;
    }
    const double span = newer->t - older->t;
    const float f = span > 0.0 ? static_cast<float>(za::clamp((ip.rt - older->t) / span, 0.0, 1.0)) : 1.f;
    for(int j = 0; j < 3; j++)
    {
        origin[j] = older->origin[j] + f * (newer->origin[j] - older->origin[j]);
    }
    // Turned as it is drawn: an alias model's pitch inverted (held::axesFromAngles). (mpopen: slerped as view angles, a
    // prop lying at a pitch of 90 degrees -- whose server angles take either of its two equal readings tick to tick --
    // was drawn spinning between two different turns: mp_ragsync's crate 143 degrees off, p95 3.4, in 1 run of 5.)
    const float sign = ent.model && ent.model->type == mod_alias ? -1.f : 1.f;
    const glm::vec3 a = mpavatar::slerpAngles(glm::vec3{sign * older->angles[0], older->angles[1], older->angles[2]},
        glm::vec3{sign * newer->angles[0], newer->angles[1], newer->angles[2]}, f);
    angles[0] = sign * a.x;
    angles[1] = a.y;
    angles[2] = a.z;
    return true;
}

double interpTime()
{
    return seatSnapInterp().interp.rt;
}

double interpDelayMs()
{
    return seatSnapInterp().interp.delay * 1000.0;
}

int interpHolds()
{
    return seatSnapInterp().interp.holds;
}

double interpNewest()
{
    return seatSnapInterp().interp.newest;
}

bool interpWay(int num, int& way, int& samples, int& restarts, int& why)
{
    const Interp& ip = seatSnapInterp().interp;
    if(num <= 0 || num >= static_cast<int>(ip.of.size()))
    {
        return false;
    }
    const History& h = ip.of[static_cast<za::SizeT>(num)];
    way = h.way;
    samples = h.n;
    restarts = h.restarts;
    why = h.restartWhy;
    return true;
}

bool interpSample(int num, int back, double& t, float origin[3])
{
    const Interp& ip = seatSnapInterp().interp;
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
    ZA_MEMCPY(origin, s.origin, sizeof(s.origin));
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
// seat-shared: the self-test's scratch
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
        if(x.key != y.key || x.len != y.len || ZA_MEMCMP(a.data(x), b.data(y), static_cast<za::SizeT>(x.len)) != 0)
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
    encode(base, cur, &buf, budget, t.sent, selftestFlags, nullptr);
    bytes = buf.cursize;
    net_message = buf;
    MSG_BeginReading();
    return decode(base, buf.cursize, t.read, selftestFlags, nullptr) && sameSnapshot(t.read, t.sent);
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
        encode(&t.base, t.cur, &buf, 4096, t.sent, selftestFlags, nullptr, prio, unsent, 40);
        const int bytes = buf.cursize;
        net_message = buf;
        MSG_BeginReading();
        bool rateOk = n <= 64 && decode(&t.base, buf.cursize, t.read, selftestFlags, nullptr) && sameSnapshot(t.read, t.sent);
        int waited = 0;
        for(za::SizeT i = 0; i < n && i < 64; i++)
        {
            waited += unsent[i];
        }
        rateOk = rateOk && !unsent[0] && !unsent[30] && waited >= static_cast<int>(n) - 6 && bytes <= 40 &&
                 ZA_MEMCMP(t.sent.data(t.sent.records[30]), t.cur.data(t.cur.records[30]), 4) == 0 &&
                 ZA_MEMCMP(t.sent.data(t.sent.records[29]), t.base.data(t.base.records[29]), 4) == 0;
        check(rateOk, va("a rate's budget of 40 bytes: the player state and the highest priority sent, %d of %d waited (%d bytes)",
                           waited, static_cast<int>(n), bytes));
    }
    {
        // B12c: the cosmetic fragments' own budget. Every record changed, the odd ones cosmetic (the later, the higher
        // their priority), 30 bytes for them: every other record goes; the cosmetic ones within 30 bytes, the highest
        // priority among them sent, the rest wait as the base's; read back the same
        t.cur = t.base;
        const za::SizeT n = t.cur.records.size();
        float prio[64]{};
        za::U8 unsent[64]{};
        za::U8 cosm[64]{};
        int last = -1;
        for(za::SizeT i = 0; i < n && i < 64; i++)
        {
            t.cur.bytes[t.cur.records[i].at] ^= 0x01;
            const bool self = t.cur.records[i].key == clientdataKey;
            prio[i] = self ? INFINITY : 1.f + static_cast<float>(i);
            cosm[i] = !self && i % 2 == 1 ? 1 : 0;
            last = cosm[i] ? static_cast<int>(i) : last;
        }
        sizebuf_t buf{};
        buf.data = t.data;
        buf.maxsize = sizeof(t.data);
        encode(&t.base, t.cur, &buf, 4096, t.sent, selftestFlags, nullptr, prio, unsent, -1, cosm, 30);
        const int cosmeticBytes = cosmeticWritten;
        net_message = buf;
        MSG_BeginReading();
        bool cosOk = n <= 64 && last > 0 && decode(&t.base, buf.cursize, t.read, selftestFlags, nullptr) && sameSnapshot(t.read, t.sent);
        int others = 0, cosWaited = 0, cosSent = 0;
        for(za::SizeT i = 0; i < n && i < 64; i++)
        {
            others += !cosm[i] && unsent[i] ? 1 : 0;
            cosWaited += cosm[i] && unsent[i] ? 1 : 0;
            cosSent += cosm[i] && !unsent[i] ? 1 : 0;
        }
        cosOk = cosOk && others == 0 && cosSent > 0 && cosWaited > 0 && cosmeticBytes <= 30 && !unsent[last];
        check(cosOk, va("the cosmetic fragments' budget of 30 bytes: every other record sent (%d waited), %d of them sent in %d "
                        "bytes (the highest priority among them), %d waited",
                         others, cosSent, cosmeticBytes, cosWaited));
        // B18: their fill. A budget of 120 bytes under a fill the other records already pass: a quarter of it (30);
        // under a fill far above them: more than 30, within the 120; every other record sent in both
        int fillBytes[2]{}, fillOthers = 0;
        for(int k = 0; k < 2; k++)
        {
            ZA_MEMSET(unsent, 0, sizeof(unsent));
            buf.cursize = 0;
            encode(&t.base, t.cur, &buf, 4096, t.sent, selftestFlags, nullptr, prio, unsent, -1, cosm, 120, k == 0 ? 1 : 100000);
            fillBytes[k] = cosmeticWritten;
            for(za::SizeT i = 0; i < n && i < 64; i++)
            {
                fillOthers += !cosm[i] && unsent[i] ? 1 : 0;
            }
        }
        check(fillOthers == 0 && fillBytes[0] > 0 && fillBytes[0] <= 30 && fillBytes[1] > 30 && fillBytes[1] <= 120,
            va("the cosmetic fragments' fill: a quarter of their 120 bytes past it (%d), more under it (%d); every other "
               "record sent (%d waited)", fillBytes[0], fillBytes[1], fillOthers));
    }
    {
        // (ragsync2) A ragdoll's record goes only in the room the others leave: every record changed, a new ragdoll's
        // (240 bytes) with the highest priority, a soft budget all the others fit in but not it too: none of them waits,
        // the ragdoll does; read back the same
        t.cur = t.base;
        const za::SizeT n = t.cur.records.size();
        float prio[65]{};
        za::U8 unsent[65]{};
        for(za::SizeT i = 0; i < n && i < 64; i++)
        {
            t.cur.bytes[t.cur.records[i].at] ^= 0x01;
            prio[i] = t.cur.records[i].key == clientdataKey ? INFINITY : 1.f;
        }
        sizebuf_t buf{};
        buf.data = t.data;
        buf.maxsize = sizeof(t.data);
        encode(&t.base, t.cur, &buf, 4096, t.sent, selftestFlags, nullptr, prio, unsent, -1);
        const int othersBytes = buf.cursize;
        za::U8 rag[240];
        for(int k = 0; k < 240; k++)
        {
            rag[k] = static_cast<za::U8>(k * 7 + 1);
        }
        t.cur.add(ragKey + 300, rag, 240);
        t.cur.sortByKey();
        const za::SizeT ri = t.cur.records.size() - 1;
        bool ragOk = n < 64 && t.cur.records[ri].key == ragKey + 300;
        prio[ri] = 100.f;
        ZA_MEMSET(unsent, 0, sizeof(unsent));
        buf.cursize = 0;
        encode(&t.base, t.cur, &buf, 4096, t.sent, selftestFlags, nullptr, prio, unsent, othersBytes + 3 * static_cast<int>(n) + 8);
        net_message = buf;
        MSG_BeginReading();
        ragOk = ragOk && decode(&t.base, buf.cursize, t.read, selftestFlags, nullptr) && sameSnapshot(t.read, t.sent);
        int othersWaited = 0;
        for(za::SizeT i = 0; i < ri; i++)
        {
            othersWaited += unsent[i];
        }
        ragOk = ragOk && othersWaited == 0 && unsent[ri];
        check(ragOk, va("a ragdoll's record (the highest priority) in the room the others leave: %d of %d others waited, the "
                        "ragdoll %s", othersWaited, static_cast<int>(n), unsent[ri] ? "waited" : "went"));
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

        // a gib flying and tumbling (every origin and angle changing by as much each frame), the base 2 frames past its
        // older, the current 1 past the base: against its trend (where its last change carries it) it is nearly free
        const float go[3][3] = {{100.f, 40.f, 80.f}, {124.f, 36.f, 86.f}, {136.f, 34.f, 89.f}};
        const float ga[3][3] = {{10.f, 20.f, 30.f}, {50.f, 60.f, 10.f}, {70.f, 80.f, 0.f}};
        const int gibBits = U_ORIGIN1 | U_ORIGIN2 | U_ORIGIN3 | U_ANGLE1 | U_ANGLE2 | U_ANGLE3;
        Snapshot older;
        for(int i = 0; i < 3; i++)
        {
            const int len = entityUpdate(u[i], gibBits, 77, go[i], ga[i], 0, 0, 0, 0);
            Snapshot& s = i == 0 ? older : i == 1 ? t.base : t.cur;
            s.clear();
            s.add(77, u[i], len);
        }
        const Trend trend{&older, 1, 2};
        int sizes[2]{};
        bool trendOk = true;
        for(int k = 0; k < 2; k++)
        {
            sizebuf_t buf{};
            buf.data = t.data;
            buf.maxsize = sizeof(t.data);
            encode(&t.base, t.cur, &buf, 4096, t.sent, selftestFlags, k ? &trend : nullptr);
            sizes[k] = buf.cursize;
            net_message = buf;
            MSG_BeginReading();
            trendOk = trendOk && decode(&t.base, buf.cursize, t.read, selftestFlags, k ? &trend : nullptr) &&
                      sameSnapshot(t.read, t.cur);
        }
        check(trendOk && sizes[1] < sizes[0] && sizes[1] <= 4,
            va("a gib flying and tumbling, against its trend: read back the same in %d bytes (against the base: %d)", sizes[1],
                sizes[0]));
    }
    net_message = savedMsg;
    msg_readcount = savedCount;
    msg_badread = savedBad;
    tally.verdict("netsnap selftest");
}

// vr_net_classes [reset] (the server): its snapshots' entity entries to every remote client since the reset, by the
// entities' classnames, the costliest first ("mp_netclass: class=... bytes=... entries=... whole=... ents=...", then
// "mp_netclass: done"): what a gib storm's bytes are (B12c). A slot reused by another thing counts as what is in it now.
void classesReport()
{
    struct ByClass
    {
        const char* name{""};
        long long bytes{0};
        int entries{0}, whole{0}, ents{0};
    };
    za::Vector<ByClass> by;
    if(sv.active)
    {
        const progs::ServerVm vm;
        for(za::SizeT n = 1; n < sentByEnt.size() && static_cast<int>(n) < qcvm->num_edicts; n++)
        {
            const SentTally& t = sentByEnt[n];
            if(t.entries == 0)
            {
                continue;
            }
            const edict_t* ed = EDICT_NUM(static_cast<int>(n));
            const char* name = ed->free ? "(freed)" : PR_GetString(ed->v.classname);
            ByClass* hit = nullptr;
            for(ByClass& b : by)
            {
                hit = !hit && ZA_STRCMP(b.name, name) == 0 ? &b : hit;
            }
            if(!hit)
            {
                by.emplaceBack(ByClass{name});
                hit = &by.back();
            }
            hit->bytes += t.bytes;
            hit->entries += t.entries;
            hit->whole += t.whole;
            hit->ents++;
        }
        za::quickSort(by.begin(), by.end(), [](const ByClass& a, const ByClass& b) { return a.bytes > b.bytes; });
        for(za::SizeT i = 0; i < by.size() && i < 10; i++)
        {
            Con_Printf("mp_netclass: class=%s bytes=%lld entries=%d whole=%d ents=%d\n", by[i].name[0] ? by[i].name : "(none)",
                by[i].bytes, by[i].entries, by[i].whole, by[i].ents);
        }
    }
    Con_Printf("mp_netclass: done\n");
}

void init()
{
    Cmd_AddCommand("vr_netsnap_selftest", selftest_f);
    netrag::init();
    Cmd_AddCommand("vr_net_classes", [] {
        if(Cmd_Argc() > 1 && !q_strcasecmp(Cmd_Argv(1), "reset"))
        {
            sentByEnt.clear();
        }
        classesReport();
    });
    // vr_net_posewaits <slot>: (the server, B19) from now on, client slot <slot>'s pose records as they went; without
    // an argument, what went since ("mp_posewait: ent=N sent=N wait=<mean frames waited plus one> weight=<mean>")
    Cmd_AddCommand("vr_net_posewaits", [] {
        if(Cmd_Argc() > 1)
        {
            poseWaits.slot = Q_atoi(Cmd_Argv(1));
            poseWaits.byEnt.clear();
        }
        for(za::SizeT e = 0; e < poseWaits.byEnt.size(); e++)
        {
            const PoseWait& t = poseWaits.byEnt[e];
            if(t.sent > 0)
            {
                Con_Printf("mp_posewait: ent=%d sent=%lld wait=%.3f weight=%.3f\n", static_cast<int>(e), t.sent,
                    t.waitSum / static_cast<double>(t.sent), t.weightSum / static_cast<double>(t.sent));
            }
        }
        Con_Printf("mp_posewait: done\n");
    });
    // vr_net_bytes [reset]: what this client read from its server, by kind ("mp_netbytes:"; reset: from zero)
    Cmd_AddCommand("vr_net_bytes", [] {
        if(Cmd_Argc() > 1 && !q_strcasecmp(Cmd_Argv(1), "reset"))
        {
            bytesReset();
            seatSnapPoses().poseEntries.clear();
            seatSnapPoses().poseDists.clear();
            seatSnapPoses().poseWeights.clear();
        }
        bytesReport();
        if(Cmd_Argc() > 1 && !q_strcasecmp(Cmd_Argv(1), "poses"))
        {
            // (each armed humanoid's pose entries read since the reset, and its mean distance from us at them: the rate's
            // priorities, B12)
            for(za::SizeT e = 0; e < seatSnapPoses().poseEntries.size(); e++)
            {
                if(seatSnapPoses().poseEntries[e] > 0)
                {
                    Con_Printf("mp_netprio: ent=%d dist=%.0f entries=%d weight=%.3f\n", static_cast<int>(e),
                        e < seatSnapPoses().poseDists.size() ? seatSnapPoses().poseDists[e] / static_cast<float>(seatSnapPoses().poseEntries[e]) : 0.f, seatSnapPoses().poseEntries[e],
                        e < seatSnapPoses().poseWeights.size() ? seatSnapPoses().poseWeights[e] / static_cast<float>(seatSnapPoses().poseEntries[e]) : 0.f);
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
