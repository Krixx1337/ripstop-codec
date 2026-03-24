#pragma once

#define RIPSTOP_CODEC_VERSION_MAJOR 1
#define RIPSTOP_CODEC_VERSION_MINOR 0
#define RIPSTOP_CODEC_VERSION_PATCH 1
#define RIPSTOP_CODEC_VERSION \
    (RIPSTOP_CODEC_VERSION_MAJOR * 10000 + RIPSTOP_CODEC_VERSION_MINOR * 100 + RIPSTOP_CODEC_VERSION_PATCH)

#include <ripstop/Error.h>
#include <ripstop/Format.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <string>
#include <span>
#include <string_view>
#include <type_traits>
#include <vector>

namespace ripstop::codec {

[[nodiscard]] constexpr std::uint32_t version() noexcept {
    return RIPSTOP_CODEC_VERSION;
}

[[nodiscard]] constexpr std::string_view version_string() noexcept {
    return "1.0.1";
}

using ScramblerFunc = void (*)(std::span<std::uint8_t> buffer, std::uint64_t state, const Header& header);

struct ProjectOptions {
    std::uint32_t magic;
    std::uint32_t domain_id = 0;
    std::uint64_t project_secret = 0;
    std::uint8_t scramble_id = Header::ScrambleSplitMix64;
    ScramblerFunc scrambler = nullptr;
};

struct AssetOptions {
    std::uint64_t format_tag = 0;
    std::uint64_t context_seed = 0;
    std::uint64_t nonce = 0;
    std::uint16_t asset_version = 1;
    IdentityType identity_type = IdentityType::None;
    std::uint8_t padding_size = 0;
    std::string_view password = "";
    bool compress = true;
    bool scramble = true;
};

namespace detail {

inline constexpr std::uint64_t split_mix_increment = 0x9e3779b97f4a7c15ull;
inline constexpr std::uint64_t fnv_offset_basis = 0xcbf29ce484222325ull;
inline constexpr std::uint64_t fnv_prime = 0x100000001b3ull;

[[nodiscard]] constexpr std::uint64_t mix64(std::uint64_t x) {
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ull;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebull;
    return x ^ (x >> 31);
}

template <typename T>
[[nodiscard]] constexpr std::span<const std::uint8_t> as_u8_bytes(std::span<const T> data) noexcept {
    static_assert(std::is_trivially_copyable_v<T>);
    const std::span<const std::byte> bytes = std::as_bytes(data);
    return {reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size()};
}

template <typename T>
[[nodiscard]] constexpr std::span<std::uint8_t> as_writable_u8_bytes(std::span<T> data) noexcept {
    static_assert(std::is_trivially_copyable_v<T>);
    const std::span<std::byte> bytes = std::as_writable_bytes(data);
    return {reinterpret_cast<std::uint8_t*>(bytes.data()), bytes.size()};
}

} // namespace detail

[[nodiscard]] bool is_encoded(std::span<const std::uint8_t> data, std::uint32_t expected_magic) noexcept;
[[nodiscard]] Result<Header> peek_header(std::span<const std::uint8_t> data, const ProjectOptions& project);
[[nodiscard]] Result<std::vector<std::uint8_t>> encode_impl(std::span<const std::uint8_t> raw_buffer,
                                                            const ProjectOptions& project,
                                                            const AssetOptions& asset = {});
[[nodiscard]] Result<std::vector<std::uint8_t>> decode_impl(std::span<const std::uint8_t> encoded_buffer,
                                                            const ProjectOptions& project,
                                                            const AssetOptions& asset = {});
[[nodiscard]] Result<std::string> decode_to_string(std::span<const std::uint8_t> encoded_buffer,
                                                   const ProjectOptions& project,
                                                   const AssetOptions& asset = {});
[[nodiscard]] ErrorCode encode_file(const std::filesystem::path& input_path,
                                    const std::filesystem::path& output_path,
                                    const ProjectOptions& project,
                                    const AssetOptions& asset = {});
[[nodiscard]] ErrorCode decode_file(const std::filesystem::path& input_path,
                                    const std::filesystem::path& output_path,
                                    const ProjectOptions& project,
                                    const AssetOptions& asset = {});
[[nodiscard]] ErrorCode decode_into(std::span<const std::uint8_t> encoded_buffer,
                                    std::span<std::uint8_t> output,
                                    const ProjectOptions& project,
                                    const AssetOptions& asset = {});

[[nodiscard]] inline Result<std::vector<std::uint8_t>> encode(std::span<const std::uint8_t> raw_buffer,
                                                              const ProjectOptions& project,
                                                              const AssetOptions& asset = {}) {
    return encode_impl(raw_buffer, project, asset);
}

[[nodiscard]] inline Result<std::vector<std::uint8_t>> decode(std::span<const std::uint8_t> encoded_buffer,
                                                              const ProjectOptions& project,
                                                              const AssetOptions& asset = {}) {
    return decode_impl(encoded_buffer, project, asset);
}

template <typename T>
    requires std::has_unique_object_representations_v<T>
[[nodiscard]] Result<std::vector<std::uint8_t>> encode(std::span<const T> raw_buffer,
                                                       const ProjectOptions& project,
                                                       const AssetOptions& asset = {}) {
    return encode_impl(detail::as_u8_bytes(raw_buffer), project, asset);
}

template <typename T>
    requires std::has_unique_object_representations_v<T>
[[nodiscard]] Result<std::vector<std::uint8_t>> decode(std::span<const T> encoded_buffer,
                                                       const ProjectOptions& project,
                                                       const AssetOptions& asset = {}) {
    return decode_impl(detail::as_u8_bytes(encoded_buffer), project, asset);
}

template <typename T>
    requires std::is_trivially_copyable_v<T>
[[nodiscard]] ErrorCode decode_into(std::span<const std::uint8_t> encoded_buffer,
                                    std::span<T> output,
                                    const ProjectOptions& project,
                                    const AssetOptions& asset = {}) {
    return decode_into(encoded_buffer, detail::as_writable_u8_bytes(output), project, asset);
}

template <typename T>
    requires std::is_trivially_copyable_v<T>
[[nodiscard]] Result<std::vector<T>> decode_to_vector(std::span<const std::uint8_t> encoded_buffer,
                                                      const ProjectOptions& project,
                                                      const AssetOptions& asset = {}) {
    const Result<Header> header = peek_header(encoded_buffer, project);
    if (!header) {
        return Result<std::vector<T>>{.error = header.error};
    }

    if (header->uncompressed_size % sizeof(T) != 0) {
        return Result<std::vector<T>>{.error = ErrorCode::BufferTooSmall};
    }

    std::vector<T> output(header->uncompressed_size / sizeof(T));
    const ErrorCode error = decode_into(encoded_buffer, std::span<T>{output}, project, asset);
    if (error != ErrorCode::Success) {
        return Result<std::vector<T>>{.error = error};
    }

    return Result<std::vector<T>>{std::move(output)};
}

namespace utils {

template <std::uint64_t Secret, std::uint8_t Mask>
struct ObfuscatedSecret {
    std::array<std::uint8_t, sizeof(std::uint64_t)> masked_bytes{};

    consteval ObfuscatedSecret() {
        for (std::size_t i = 0; i < masked_bytes.size(); ++i) {
            const std::uint8_t byte = static_cast<std::uint8_t>((Secret >> (i * 8)) & 0xFFu);
            masked_bytes[i] = static_cast<std::uint8_t>(byte ^ Mask);
        }
    }

    [[nodiscard]] std::uint64_t resolve() const noexcept {
        volatile std::uint8_t mask = Mask;
        std::uint64_t secret = 0;

        for (std::size_t i = 0; i < masked_bytes.size(); ++i) {
            const volatile std::uint8_t masked = masked_bytes[i];
            secret |= static_cast<std::uint64_t>(masked ^ mask) << (i * 8);
        }

        return secret;
    }
};

template <std::uint64_t Secret, std::uint8_t Mask>
[[nodiscard]] consteval auto make_obfuscated_secret() {
    return ObfuscatedSecret<Secret, Mask>{};
}

[[nodiscard]] inline constexpr std::uint64_t hash_string(std::string_view value) {
    std::uint64_t hash = detail::fnv_offset_basis;

    for (unsigned char ch : value) {
        hash ^= ch;
        hash *= detail::fnv_prime;
    }

    return hash;
}

[[nodiscard]] inline constexpr std::uint64_t hash_uint64(std::uint64_t value) {
    return detail::mix64(value + detail::split_mix_increment);
}

} // namespace utils

} // namespace ripstop::codec
