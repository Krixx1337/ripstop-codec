#pragma once

#include <cstdint>

#define RIPSTOP_HAS_CUSTOM_ERROR_CODE_ENUM 1
#define RIPSTOP_HARDEN_ERRORS 1
#define RIPSTOP_ERROR_XOR 0x13572468u

namespace ripstop::codec {

enum class [[nodiscard]] ErrorCode : std::uint32_t {
    Success = 0x10000001u,
    BufferTooSmall = 0x10000002u,
    MagicMismatch = 0x10000003u,
    UnsupportedVersion = 0x10000004u,
    UnsupportedScrambleId = 0x10000005u,
    MissingScramblerFunc = 0x10000006u,
    InvalidFlags = 0x10000007u,
    InvalidIdentityType = 0x10000008u,
    SizeLimitExceeded = 0x10000009u,
    DomainMismatch = 0x1000000Au,
    UnsupportedCompression = 0x1000000Bu,
    CompressionFailed = 0x1000000Cu,
    DecompressionFailed = 0x1000000Du,
    CrcMismatch = 0x1000000Eu,
    PreFlightAbort = 0x1000000Fu,
    FileOpenFailed = 0x10000010u,
    FileReadFailed = 0x10000011u,
    FileWriteFailed = 0x10000012u,
};

} // namespace ripstop::codec
