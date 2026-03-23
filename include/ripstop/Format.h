#pragma once

#include <cstdint>
#include <type_traits>

namespace ripstop::codec {

enum class IdentityType : std::uint8_t {
    None = 0,
    String = 1,
    Integer = 2,
    Global = 3,
    Random = 4,
    CustomMin = 64
};

enum class HeaderFlags : std::uint16_t {
    None = 0,
    Compressed = 0x01, // Payload bytes are compressed before storage; see compression_id.
    Scrambled = 0x02   // Payload bytes are transformed in place with the configured scrambler.
};

enum class CompressionType : std::uint8_t {
    None = 0,
    Deflate = 1,
    Zstd = 2,
    Lz4 = 3
};

#pragma pack(push, 1)
struct Header final {
    static constexpr std::uint16_t CodecVersion = 1;
    static constexpr std::uint8_t ScrambleSplitMix64 = 0;

    std::uint32_t magic;
    std::uint32_t domain_id;
    std::uint16_t codec_version;
    std::uint16_t asset_version;
    std::uint8_t identity_type; // Library-defined values live in the low range; callers may use custom values.
    std::uint8_t scramble_id;
    HeaderFlags flags;
    std::uint32_t uncompressed_size;
    std::uint32_t compressed_size;
    std::uint32_t masked_crc;
    std::uint16_t header_size;
    std::uint8_t compression_id;
    std::uint8_t reserved;
    std::uint64_t nonce;
};
#pragma pack(pop)

static_assert(std::is_trivially_copyable_v<Header>, "RipStop header must be POD-like.");
static_assert(std::is_standard_layout_v<Header>, "RipStop header must use standard layout.");
static_assert(sizeof(Header) == 40, "Header must be 40 bytes");

} // namespace ripstop::codec
