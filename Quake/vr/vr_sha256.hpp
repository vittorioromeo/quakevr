#pragma once

// vr_sha256.hpp -- SHA-256 (FIPS 180-4), for checking a download against the hash its index gives (the Map Library's
// zips: vr_mapinstall.cpp). Portable, no dependencies; about 200 MB/s, so it runs on the job's thread.

#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Base/SizeT.hpp"

namespace qvr::sha256
{

struct Digest
{
    za::U8 bytes[32];
};

// The hash of n bytes at data.
[[nodiscard]] Digest of(const void* data, za::SizeT n);

// The digest as 64 lowercase hex digits and a NUL.
void toHex(const Digest& d, char (&out)[65]);

// Whether hex (64 hex digits, either case) names this digest.
[[nodiscard]] bool matches(const Digest& d, const char* hex);

// vr_sha256_test: the standard test vectors (FIPS 180-2's "abc", the 448-bit message, a million 'a's, and the empty
// message), one console line.
void selfTest_f();

} // namespace qvr::sha256
