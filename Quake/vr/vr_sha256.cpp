// vr_sha256.cpp -- SHA-256 (FIPS 180-4; vr_sha256.hpp).

#include "vr_sha256.hpp"
#include "vr_engine.hpp"

#include "Zancle/Container/Vector.hpp"

#include <string.h>

#if defined(_M_X64) || defined(__x86_64__)
#include <immintrin.h>
#ifdef _MSC_VER
#include <intrin.h>
#else
#include <cpuid.h>
#endif
#endif

namespace qvr::sha256
{
namespace
{

constexpr za::U32 roundK[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01,
    0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
    0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116, 0x1e376c08,
    0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

[[nodiscard]] constexpr za::U32 rotr(za::U32 x, int n)
{
    return (x >> n) | (x << (32 - n));
}

// One 64-byte block into the state.
void compress(za::U32 (&h)[8], const za::U8* block)
{
    za::U32 w[64];
    for(int i = 0; i < 16; i++)
    {
        w[i] = (static_cast<za::U32>(block[i * 4]) << 24) | (static_cast<za::U32>(block[i * 4 + 1]) << 16) |
               (static_cast<za::U32>(block[i * 4 + 2]) << 8) | static_cast<za::U32>(block[i * 4 + 3]);
    }
    for(int i = 16; i < 64; i++)
    {
        const za::U32 s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
        const za::U32 s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    za::U32 a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], k = h[7];
    for(int i = 0; i < 64; i++)
    {
        const za::U32 t1 = k + (rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25)) + ((e & f) ^ (~e & g)) + roundK[i] + w[i];
        const za::U32 t2 = (rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
        k = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }
    h[0] += a;
    h[1] += b;
    h[2] += c;
    h[3] += d;
    h[4] += e;
    h[5] += f;
    h[6] += g;
    h[7] += k;
}

#if defined(_M_X64) || defined(__x86_64__)
#if defined(__clang__) || defined(__GNUC__)
#define QVR_TARGET_SHA __attribute__((target("sha,sse4.1,ssse3")))
#else
#define QVR_TARGET_SHA
#endif

// Whether the processor has the SHA extensions (and SSSE3, SSE4.1, which the blocks below use too): Intel since Ice
// Lake and Goldmont, AMD since Zen.
[[nodiscard]] bool detectShaNi()
{
#ifdef _MSC_VER
    int info[4] = {};
    __cpuid(info, 0);
    if(info[0] < 7)
    {
        return false;
    }
    __cpuid(info, 1);
    const bool ssse3 = (info[2] & (1 << 9)) != 0, sse41 = (info[2] & (1 << 19)) != 0;
    __cpuidex(info, 7, 0);
    return ssse3 && sse41 && (info[1] & (1 << 29)) != 0;
#else
    unsigned a = 0, b = 0, c = 0, d = 0;
    if(!__get_cpuid(1, &a, &b, &c, &d))
    {
        return false;
    }
    const bool ssse3 = (c & (1u << 9)) != 0, sse41 = (c & (1u << 19)) != 0;
    if(!__get_cpuid_count(7, 0, &a, &b, &c, &d))
    {
        return false;
    }
    return ssse3 && sse41 && (b & (1u << 29)) != 0;
#endif
}

const bool shaNiPresent = detectShaNi();
bool shaNiOn = shaNiPresent; // (vr_sha256_test turns it off to compare the portable blocks)

// `count` 64-byte blocks into the state with the SHA extensions (map loads: vr_pbr's content hash of every world
// texture, 7-8 times the portable blocks' speed). The same rounds as compress(): four at a time, the state kept as
// ABEF and CDGH.
QVR_TARGET_SHA void compressShaNi(za::U32 (&h)[8], const za::U8* data, za::SizeT count)
{
    const __m128i swap = _mm_set_epi64x(0x0c0d0e0f08090a0bll, 0x0405060700010203ll); // big-endian words
    __m128i tmp = _mm_shuffle_epi32(_mm_loadu_si128(reinterpret_cast<const __m128i*>(&h[0])), 0xB1); // CDAB
    __m128i state1 = _mm_shuffle_epi32(_mm_loadu_si128(reinterpret_cast<const __m128i*>(&h[4])), 0x1B); // EFGH
    __m128i state0 = _mm_alignr_epi8(tmp, state1, 8);    // ABEF
    state1 = _mm_blend_epi16(state1, tmp, 0xF0);         // CDGH
    for(; count; count--, data += 64)
    {
        const __m128i abef = state0, cdgh = state1;
        __m128i w[4]; // the schedule's last 16 words, four at a time (w[i & 3]: words 4i to 4i + 3)
        for(int i = 0; i < 16; i++)
        {
            if(i < 4)
            {
                w[i] = _mm_shuffle_epi8(_mm_loadu_si128(reinterpret_cast<const __m128i*>(data + i * 16)), swap);
            }
            else
            {
                // W[t] = s1(W[t-2]) + W[t-7] + s0(W[t-15]) + W[t-16]
                const __m128i m = _mm_add_epi32(_mm_sha256msg1_epu32(w[i & 3], w[(i + 1) & 3]),
                    _mm_alignr_epi8(w[(i + 3) & 3], w[(i + 2) & 3], 4));
                w[i & 3] = _mm_sha256msg2_epu32(m, w[(i + 3) & 3]);
            }
            __m128i msg = _mm_add_epi32(w[i & 3], _mm_loadu_si128(reinterpret_cast<const __m128i*>(&roundK[i * 4])));
            state1 = _mm_sha256rnds2_epu32(state1, state0, msg);
            msg = _mm_shuffle_epi32(msg, 0x0E);
            state0 = _mm_sha256rnds2_epu32(state0, state1, msg);
        }
        state0 = _mm_add_epi32(state0, abef);
        state1 = _mm_add_epi32(state1, cdgh);
    }
    tmp = _mm_shuffle_epi32(state0, 0x1B);        // FEBA
    state1 = _mm_shuffle_epi32(state1, 0xB1);     // DCHG
    state0 = _mm_blend_epi16(tmp, state1, 0xF0);  // DCBA
    state1 = _mm_alignr_epi8(state1, tmp, 8);     // HGFE
    _mm_storeu_si128(reinterpret_cast<__m128i*>(&h[0]), state0);
    _mm_storeu_si128(reinterpret_cast<__m128i*>(&h[4]), state1);
}
#endif

// `count` 64-byte blocks into the state: the SHA extensions' when the processor has them, else compress().
void compressBlocks(za::U32 (&h)[8], const za::U8* data, za::SizeT count)
{
#if defined(_M_X64) || defined(__x86_64__)
    if(shaNiOn)
    {
        compressShaNi(h, data, count);
        return;
    }
#endif
    for(; count; count--, data += 64)
    {
        compress(h, data);
    }
}

[[nodiscard]] int hexDigit(char c)
{
    if(c >= '0' && c <= '9')
    {
        return c - '0';
    }
    if(c >= 'a' && c <= 'f')
    {
        return c - 'a' + 10;
    }
    if(c >= 'A' && c <= 'F')
    {
        return c - 'A' + 10;
    }
    return -1;
}

} // namespace

Digest of(const void* data, za::SizeT n)
{
    Hasher hasher;
    hasher.update(data, n);
    return hasher.finish();
}

void Hasher::update(const void* data, za::SizeT n)
{
    const za::U8* p = static_cast<const za::U8*>(data);
    total += n;
    if(held)
    {
        const za::SizeT take = n < 64 - held ? n : 64 - held;
        memcpy(block + held, p, take);
        held += take;
        p += take;
        n -= take;
        if(held < 64)
        {
            return;
        }
        compressBlocks(h, block, 1);
        held = 0;
    }
    if(n >= 64)
    {
        compressBlocks(h, p, n / 64);
        p += n / 64 * 64;
        n %= 64;
    }
    if(n)
    {
        memcpy(block, p, n);
        held = n;
    }
}

Digest Hasher::finish()
{
    // The tail, the 0x80 byte, zeros, and the length in bits (big-endian): one block or two.
    za::U8 tail[128] = {};
    if(held)
    {
        memcpy(tail, block, held);
    }
    tail[held] = 0x80;
    const za::SizeT tailBytes = held + 1 + 8 <= 64 ? 64 : 128;
    const za::U64 bits = total * 8;
    for(int i = 0; i < 8; i++)
    {
        tail[tailBytes - 1 - i] = static_cast<za::U8>(bits >> (i * 8));
    }
    compressBlocks(h, tail, tailBytes / 64);
    Digest d;
    for(int i = 0; i < 8; i++)
    {
        d.bytes[i * 4] = static_cast<za::U8>(h[i] >> 24);
        d.bytes[i * 4 + 1] = static_cast<za::U8>(h[i] >> 16);
        d.bytes[i * 4 + 2] = static_cast<za::U8>(h[i] >> 8);
        d.bytes[i * 4 + 3] = static_cast<za::U8>(h[i]);
    }
    return d;
}

void toHex(const Digest& d, char (&out)[65])
{
    constexpr char digits[] = "0123456789abcdef";
    for(int i = 0; i < 32; i++)
    {
        out[i * 2] = digits[d.bytes[i] >> 4];
        out[i * 2 + 1] = digits[d.bytes[i] & 15];
    }
    out[64] = '\0';
}

bool matches(const Digest& d, const char* hex)
{
    if(!hex || strlen(hex) != 64)
    {
        return false;
    }
    for(int i = 0; i < 32; i++)
    {
        const int hi = hexDigit(hex[i * 2]);
        const int lo = hexDigit(hex[i * 2 + 1]);
        if(hi < 0 || lo < 0 || d.bytes[i] != static_cast<za::U8>(hi * 16 + lo))
        {
            return false;
        }
    }
    return true;
}

void selfTest_f()
{
    struct Vector
    {
        const char* text;
        int repeat;
        const char* hex;
    };
    constexpr Vector vectors[] = {
        {"abc", 1, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"},
        {"", 1, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"},
        {"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", 1,
            "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"},
        {"a", 1000000, "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0"},
        // 55 and 56 bytes: the padding fits one block, then needs two.
        {"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", 1,
            "9f4390f8d30c2dd92ec9f095b65e2b9ae9b0a925a5258e241c9f1e910f734318"},
        {"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", 1,
            "b35439a4ac6f0948b6d6f9e3c6af0f5f590ce20f1bde7090ef7970686ec6738a"},
    };
    int passed = 0;
    int total = 0;
    for(const Vector& v : vectors)
    {
        total++;
        const za::SizeT len = strlen(v.text);
        Digest d;
        if(v.repeat == 1)
        {
            d = of(v.text, len);
        }
        else
        {
            za::Vector<char> buf;
            buf.resize(len * static_cast<za::SizeT>(v.repeat));
            for(int i = 0; i < v.repeat; i++)
            {
                memcpy(buf.data() + i * len, v.text, len);
            }
            d = of(buf.data(), buf.size());
        }
        if(matches(d, v.hex))
        {
            passed++;
        }
        else
        {
            char got[65];
            toHex(d, got);
            Con_Printf("vr_sha256_test: \"%.20s\" x%d: %s, expected %s\n", v.text, v.repeat, got, v.hex);
        }
    }
    // The same vectors fed in uneven pieces (the Hasher a download is checked with: 1 to 97 bytes at a time).
    for(const Vector& v : vectors)
    {
        total++;
        const za::SizeT len = strlen(v.text);
        za::Vector<char> buf;
        buf.resize(len * static_cast<za::SizeT>(v.repeat));
        for(int i = 0; i < v.repeat; i++)
        {
            memcpy(buf.data() + i * len, v.text, len);
        }
        Hasher hasher;
        za::SizeT at = 0;
        za::SizeT piece = 1;
        while(at < buf.size())
        {
            const za::SizeT take = buf.size() - at < piece ? buf.size() - at : piece;
            hasher.update(buf.data() + at, take);
            at += take;
            piece = piece % 97 + 1;
        }
        if(matches(hasher.finish(), v.hex))
        {
            passed++;
        }
        else
        {
            Con_Printf("vr_sha256_test: \"%.20s\" x%d in pieces: wrong\n", v.text, v.repeat);
        }
    }
#if defined(_M_X64) || defined(__x86_64__)
    // The SHA extensions' blocks against the portable ones: pseudo-random data of every length to 300 bytes and 4 MB
    // (a 1024 x 1024 texture), the same digests; and both speeds.
    if(shaNiPresent)
    {
        za::Vector<za::U8> data;
        data.resize(4u << 20);
        za::U32 x = 0x9E3779B9u;
        for(za::U8& b : data)
        {
            x = x * 1664525u + 1013904223u;
            b = static_cast<za::U8>(x >> 24);
        }
        int same = 0, lengths = 0;
        double niMs = 0.0, portableMs = 0.0;
        for(za::SizeT len = 0; len <= 301; len++)
        {
            const za::SizeT n = len == 301 ? data.size() : len;
            lengths++;
            shaNiOn = true;
            double t0 = Sys_DoubleTime();
            const Digest a = of(data.data(), n);
            niMs += (Sys_DoubleTime() - t0) * 1000.0;
            shaNiOn = false;
            t0 = Sys_DoubleTime();
            const Digest b = of(data.data(), n);
            portableMs += (Sys_DoubleTime() - t0) * 1000.0;
            same += memcmp(a.bytes, b.bytes, sizeof(a.bytes)) == 0;
        }
        shaNiOn = true;
        total++;
        passed += same == lengths;
        Con_Printf("vr_sha256_test: SHA extensions against the portable blocks: %d/%d lengths the same; %.2f against "
                   "%.2f ms\n", same, lengths, niMs, portableMs);
    }
    else
    {
        Con_Printf("vr_sha256_test: no SHA extensions on this processor (the portable blocks)\n");
    }
#endif
    Con_Printf("vr_sha256_test: %d/%d vectors %s\n", passed, total, passed == total ? "match" : "FAILED");
}

} // namespace qvr::sha256
