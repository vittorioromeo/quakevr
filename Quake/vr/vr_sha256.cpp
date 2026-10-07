// vr_sha256.cpp -- SHA-256 (FIPS 180-4; vr_sha256.hpp).

#include "vr_sha256.hpp"
#include "vr_engine.hpp"

#include "Zancle/Container/Vector.hpp"

#include <string.h>

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
        compress(h, block);
        held = 0;
    }
    while(n >= 64)
    {
        compress(h, p);
        p += 64;
        n -= 64;
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
    compress(h, tail);
    if(tailBytes == 128)
    {
        compress(h, tail + 64);
    }
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
    Con_Printf("vr_sha256_test: %d/%d vectors %s\n", passed, total, passed == total ? "match" : "FAILED");
}

} // namespace qvr::sha256
