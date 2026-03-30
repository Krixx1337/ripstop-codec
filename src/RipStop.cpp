#include <ripstop/Codec.h>

#include <algorithm>
#include <bit>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>

extern "C" {
#include <miniz/miniz.h>
}

namespace ripstop::codec {
namespace {

static_assert(std::endian::native == std::endian::little,
              "RipStop Codec currently requires a little-endian architecture.");

constexpr std::uint32_t k_max_payload_size = 256u * 1024u * 1024u;
constexpr std::uint16_t k_known_flags_mask =
    static_cast<std::uint16_t>(HeaderFlags::Compressed) | static_cast<std::uint16_t>(HeaderFlags::Scrambled);

template <typename T>
[[nodiscard]] Result<T> make_error(ErrorCode error) {
    Security::OnError(error);
    return Result<T>{.error = error};
}

template <typename T>
[[nodiscard]] Result<T> make_tamper_error(ErrorCode error) {
    Security::OnError(error);
    Security::OnTamper(error);
    return Result<T>{.error = error};
}

[[nodiscard]] ErrorCode report_error(ErrorCode error) {
    if (error != ErrorCode::Success) {
        Security::OnError(error);
    }

    return error;
}

[[nodiscard]] ErrorCode report_tamper_error(ErrorCode error) {
    Security::OnError(error);
    Security::OnTamper(error);
    return error;
}

} // namespace

std::string to_string(ErrorCode error) {
#if RIPSTOP_HARDEN_ERRORS
    return ::HOSTILE_CORE_NAMESPACE::harden_error_code(error, 0u);
#else
    switch (error) {
    case ErrorCode::Success: return "Success";
    case ErrorCode::BufferTooSmall: return "BufferTooSmall";
    case ErrorCode::MagicMismatch: return "MagicMismatch";
    case ErrorCode::UnsupportedVersion: return "UnsupportedVersion";
    case ErrorCode::UnsupportedScrambleId: return "UnsupportedScrambleId";
    case ErrorCode::MissingScramblerFunc: return "MissingScramblerFunc";
    case ErrorCode::InvalidFlags: return "InvalidFlags";
    case ErrorCode::InvalidIdentityType: return "InvalidIdentityType";
    case ErrorCode::SizeLimitExceeded: return "SizeLimitExceeded";
    case ErrorCode::DomainMismatch: return "DomainMismatch";
    case ErrorCode::UnsupportedCompression: return "UnsupportedCompression";
    case ErrorCode::CompressionFailed: return "CompressionFailed";
    case ErrorCode::DecompressionFailed: return "DecompressionFailed";
    case ErrorCode::CrcMismatch: return "CrcMismatch";
    case ErrorCode::PreFlightAbort: return "PreFlightAbort";
    case ErrorCode::FileOpenFailed: return "FileOpenFailed";
    case ErrorCode::FileReadFailed: return "FileReadFailed";
    case ErrorCode::FileWriteFailed: return "FileWriteFailed";
    }

    return "UnknownError";
#endif
}

namespace {

[[nodiscard]] bool has_flag(HeaderFlags value, HeaderFlags flag) {
    return (static_cast<std::uint16_t>(value) & static_cast<std::uint16_t>(flag)) != 0;
}

void set_flag(HeaderFlags& value, HeaderFlags flag) {
    value = static_cast<HeaderFlags>(static_cast<std::uint16_t>(value) | static_cast<std::uint16_t>(flag));
}

[[nodiscard]] mz_ulong mz_ulong_limit() {
    return (std::numeric_limits<mz_ulong>::max)();
}

[[nodiscard]] ErrorCode validate_size(std::size_t size) {
    if (size > std::numeric_limits<std::uint32_t>::max()) {
        return ErrorCode::SizeLimitExceeded;
    }

    if (size > k_max_payload_size) {
        return ErrorCode::SizeLimitExceeded;
    }

    return ErrorCode::Success;
}

[[nodiscard]] bool is_known_compression(CompressionType type) {
    switch (type) {
    case CompressionType::None:
    case CompressionType::Deflate:
    case CompressionType::Zstd:
    case CompressionType::Lz4:
        return true;
    }

    return false;
}

[[nodiscard]] Result<mz_ulong> to_mz_ulong(std::size_t size) {
    if (size > static_cast<std::size_t>(mz_ulong_limit())) {
        return make_error<mz_ulong>(ErrorCode::SizeLimitExceeded);
    }

    return Result<mz_ulong>{static_cast<mz_ulong>(size)};
}

[[nodiscard]] Result<std::vector<std::uint8_t>> compress_payload(std::span<const std::uint8_t> data,
                                                                 CompressionType type) {
    switch (type) {
    case CompressionType::None:
        return Result<std::vector<std::uint8_t>>{std::vector<std::uint8_t>(data.begin(), data.end())};
    case CompressionType::Deflate: {
        const Result<mz_ulong> input_size = to_mz_ulong(data.size());
        if (!input_size) {
            return make_error<std::vector<std::uint8_t>>(input_size.error);
        }

        mz_ulong compressed_size = mz_compressBound(input_size.value);
        std::vector<std::uint8_t> compressed_payload(compressed_size);
        const int result = mz_compress(compressed_payload.data(), &compressed_size, data.data(), input_size.value);
        if (result != MZ_OK) {
            return make_error<std::vector<std::uint8_t>>(ErrorCode::CompressionFailed);
        }

        compressed_payload.resize(static_cast<std::size_t>(compressed_size));
        if (const ErrorCode size_error = validate_size(compressed_payload.size()); size_error != ErrorCode::Success) {
            return make_error<std::vector<std::uint8_t>>(size_error);
        }

        return Result<std::vector<std::uint8_t>>{std::move(compressed_payload)};
    }
    case CompressionType::Zstd:
    case CompressionType::Lz4:
        return make_error<std::vector<std::uint8_t>>(ErrorCode::UnsupportedCompression);
    }

    return make_error<std::vector<std::uint8_t>>(ErrorCode::UnsupportedCompression);
}

[[nodiscard]] ErrorCode decompress_payload(std::span<const std::uint8_t> compressed_input,
                                           std::span<std::uint8_t> output,
                                           CompressionType type) {
    switch (type) {
    case CompressionType::None:
        if (compressed_input.size() != output.size()) {
            return ErrorCode::BufferTooSmall;
        }

        if (!compressed_input.empty()) {
            std::memcpy(output.data(), compressed_input.data(), compressed_input.size());
        }

        return ErrorCode::Success;
    case CompressionType::Deflate: {
        const Result<mz_ulong> output_size = to_mz_ulong(output.size());
        if (!output_size) {
            return output_size.error;
        }

        const Result<mz_ulong> compressed_size = to_mz_ulong(compressed_input.size());
        if (!compressed_size) {
            return compressed_size.error;
        }

        mz_ulong decoded_size = output_size.value;
        const int result = mz_uncompress(output.data(), &decoded_size, compressed_input.data(), compressed_size.value);
        if (result != MZ_OK) {
            return ErrorCode::DecompressionFailed;
        }

        if (decoded_size != output_size.value) {
            return ErrorCode::BufferTooSmall;
        }

        return ErrorCode::Success;
    }
    case CompressionType::Zstd:
    case CompressionType::Lz4:
        return ErrorCode::UnsupportedCompression;
    }

    return ErrorCode::UnsupportedCompression;
}

[[nodiscard]] std::uint64_t mix64(std::uint64_t x) {
    return detail::mix64(x);
}

[[nodiscard]] std::uint32_t compute_crc32(std::span<const std::uint8_t> buffer) {
    const void* data = buffer.empty() ? nullptr : buffer.data();
    return static_cast<std::uint32_t>(
        mz_crc32(MZ_CRC32_INIT, static_cast<const unsigned char*>(data), buffer.size()));
}

[[nodiscard]] std::uint32_t crc_mask(const ProjectOptions& project) {
    return static_cast<std::uint32_t>(project.project_secret & 0xFFFFFFFFull);
}

[[nodiscard]] std::uint32_t mask_crc(std::uint32_t crc, const ProjectOptions& project) {
    return crc ^ crc_mask(project);
}

[[nodiscard]] std::uint32_t unmask_crc(std::uint32_t masked_crc, const ProjectOptions& project) {
    return masked_crc ^ crc_mask(project);
}

[[nodiscard]] std::uint64_t make_entropy_seed(std::size_t salt) {
    const auto now = static_cast<std::uint64_t>(
        std::chrono::high_resolution_clock::now().time_since_epoch().count());
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(&salt));
    return detail::mix64(now ^ address ^ static_cast<std::uint64_t>(salt));
}

void fill_with_noise(std::span<std::uint8_t> buffer, std::uint64_t seed) {
    std::uint64_t state = seed;
    for (std::uint8_t& byte : buffer) {
        state += detail::split_mix_increment;
        byte = static_cast<std::uint8_t>(mix64(state) & 0xFFu);
    }
}

void default_scrambler(std::span<std::uint8_t> buffer, std::uint64_t state, const Header&) {
    for (std::uint8_t& byte : buffer) {
        state += detail::split_mix_increment;
        const std::uint64_t mixed = mix64(state);
        byte ^= static_cast<std::uint8_t>(mixed & 0xFFu);
    }
}

[[nodiscard]] ScramblerFunc resolve_scrambler(const ProjectOptions& project) {
    if (project.scrambler != nullptr) {
        return project.scrambler;
    }

    if (project.scramble_id == Header::ScrambleSplitMix64) {
        return &default_scrambler;
    }

    return nullptr;
}

[[nodiscard]] std::uint64_t derive_scramble_state(const ProjectOptions& project,
                                                  const AssetOptions& asset,
                                                  const Header& header) {
    std::uint64_t state = utils::hash_uint64(project.project_secret) ^ utils::hash_uint64(asset.context_seed) ^
                          utils::hash_uint64(asset.format_tag);
    state ^= utils::hash_uint64(header.nonce);

    if (!asset.password.empty()) {
        state ^= utils::hash_string(asset.password);
    }

    Security::OnScrambleState(state);
    return mix64(state);
}

[[nodiscard]] ErrorCode transform_in_place(std::span<std::uint8_t> buffer,
                                           const ProjectOptions& project,
                                           const AssetOptions& asset,
                                           const Header& header) {
    const ScramblerFunc scrambler = resolve_scrambler(project);
    if (scrambler == nullptr) {
        return ErrorCode::MissingScramblerFunc;
    }

    scrambler(buffer, derive_scramble_state(project, asset, header), header);
    return ErrorCode::Success;
}

void transform_header(Header& header, const ProjectOptions& project) {
    std::uint64_t mask = utils::hash_uint64(project.project_secret ^ project.domain_id);
    auto* header_bytes = reinterpret_cast<std::uint8_t*>(&header);

    for (std::size_t i = sizeof(header.magic); i < sizeof(Header); ++i) {
        const std::size_t mask_index = (i - sizeof(header.magic)) % sizeof(mask);
        header_bytes[i] ^= static_cast<std::uint8_t>((mask >> (mask_index * 8)) & 0xFFu);
    }
}

[[nodiscard]] Result<Header> parse_header(std::span<const std::uint8_t> encoded_buffer, const ProjectOptions& project) {
    if (encoded_buffer.size() < sizeof(Header)) {
        return make_error<Header>(ErrorCode::BufferTooSmall);
    }

    Header header{};
    std::memcpy(&header, encoded_buffer.data(), sizeof(header));
    transform_header(header, project);

    if (header.magic != project.magic) {
        return make_tamper_error<Header>(ErrorCode::MagicMismatch);
    }

    if (header.codec_version > Header::CodecVersion) {
        return make_error<Header>(ErrorCode::UnsupportedVersion);
    }

    if (header.header_size < sizeof(Header)) {
        return make_error<Header>(ErrorCode::UnsupportedVersion);
    }

    if ((static_cast<std::uint16_t>(header.flags) & ~k_known_flags_mask) != 0) {
        return make_error<Header>(ErrorCode::InvalidFlags);
    }

    const CompressionType compression = static_cast<CompressionType>(header.compression_id);
    if (!is_known_compression(compression)) {
        return make_error<Header>(ErrorCode::UnsupportedCompression);
    }

    if (has_flag(header.flags, HeaderFlags::Compressed) != (compression != CompressionType::None)) {
        return make_error<Header>(ErrorCode::UnsupportedCompression);
    }

    if (const ErrorCode size_error = validate_size(header.uncompressed_size); size_error != ErrorCode::Success) {
        return make_error<Header>(size_error);
    }

    if (const ErrorCode size_error = validate_size(header.compressed_size); size_error != ErrorCode::Success) {
        return make_error<Header>(size_error);
    }

    const std::size_t declared_payload_size = has_flag(header.flags, HeaderFlags::Compressed)
                                                  ? header.compressed_size
                                                  : header.uncompressed_size;

    const std::size_t total_size = static_cast<std::size_t>(header.header_size) + declared_payload_size;
    if (encoded_buffer.size() != total_size) {
        return make_error<Header>(ErrorCode::BufferTooSmall);
    }

    return Result<Header>{header};
}

[[nodiscard]] Result<std::vector<std::uint8_t>> read_file_bytes(const std::filesystem::path& input_path) {
    std::ifstream input(input_path, std::ios::binary | std::ios::ate);
    if (!input) {
        return make_error<std::vector<std::uint8_t>>(ErrorCode::FileOpenFailed);
    }

    const std::ifstream::pos_type end_pos = input.tellg();
    if (end_pos < 0) {
        return make_error<std::vector<std::uint8_t>>(ErrorCode::FileReadFailed);
    }

    const std::size_t size = static_cast<std::size_t>(end_pos);
    if (const ErrorCode size_error = validate_size(size); size_error != ErrorCode::Success) {
        return make_error<std::vector<std::uint8_t>>(size_error);
    }

    std::vector<std::uint8_t> data(size);
    input.seekg(0, std::ios::beg);
    if (!input) {
        return make_error<std::vector<std::uint8_t>>(ErrorCode::FileReadFailed);
    }

    if (!data.empty() && !input.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size()))) {
        return make_error<std::vector<std::uint8_t>>(ErrorCode::FileReadFailed);
    }

    return Result<std::vector<std::uint8_t>>{std::move(data)};
}

[[nodiscard]] ErrorCode write_file_bytes(const std::filesystem::path& output_path,
                                         std::span<const std::uint8_t> data) {
    std::ofstream output(output_path, std::ios::binary | std::ios::trunc);
    if (!output) {
        return ErrorCode::FileOpenFailed;
    }

    if (!data.empty() && !output.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()))) {
        return ErrorCode::FileWriteFailed;
    }

    if (!output) {
        return ErrorCode::FileWriteFailed;
    }

    return ErrorCode::Success;
}

[[nodiscard]] ErrorCode decode_payload_into(std::span<const std::uint8_t> payload,
                                            const Header& header,
                                            std::span<std::uint8_t> output,
                                            const ProjectOptions& project,
                                            const AssetOptions& asset) {
    if (output.size() != header.uncompressed_size) {
        return ErrorCode::BufferTooSmall;
    }

    if (header.scramble_id != project.scramble_id) {
        return ErrorCode::UnsupportedScrambleId;
    }

    const CompressionType compression = static_cast<CompressionType>(header.compression_id);
    if (compression == CompressionType::None) {
        const ErrorCode decompress_error = decompress_payload(payload, output, compression);
        if (decompress_error != ErrorCode::Success) {
            return report_error(decompress_error);
        }

        if (has_flag(header.flags, HeaderFlags::Scrambled)) {
            if (const ErrorCode error = transform_in_place(output, project, asset, header);
                error != ErrorCode::Success) {
                return error;
            }
            if (!Security::PostDescramble(output)) {
                return report_error(ErrorCode::PreFlightAbort);
            }
        }

        if (compute_crc32(output) != unmask_crc(header.masked_crc, project)) {
            return report_tamper_error(ErrorCode::CrcMismatch);
        }

        return ErrorCode::Success;
    }

    if (const ErrorCode size_error = validate_size(header.uncompressed_size); size_error != ErrorCode::Success) {
        return size_error;
    }

    std::span<const std::uint8_t> compressed_input = payload;
    std::vector<std::uint8_t> temp_payload;

    if (has_flag(header.flags, HeaderFlags::Scrambled)) {
        // Guardrail: compressed decode cannot descramble into `output` and then ask miniz
        // to read from that same buffer. zlib-style decompress is not an in-place contract.
        temp_payload.assign(payload.begin(), payload.end());
        if (const ErrorCode error = transform_in_place(temp_payload, project, asset, header);
            error != ErrorCode::Success) {
            SecureWipe(temp_payload);
            return report_error(error);
        }
        if (!Security::PostDescramble(temp_payload)) {
            SecureWipe(temp_payload);
            return report_error(ErrorCode::PreFlightAbort);
        }
        compressed_input = temp_payload;
    }

    if (const ErrorCode decompress_error = decompress_payload(compressed_input, output, compression);
        decompress_error != ErrorCode::Success) {
        SecureWipe(temp_payload);
        return report_error(decompress_error);
    }

    if (compute_crc32(output) != unmask_crc(header.masked_crc, project)) {
        SecureWipe(temp_payload);
        return report_tamper_error(ErrorCode::CrcMismatch);
    }

    SecureWipe(temp_payload);
    return ErrorCode::Success;
}

} // namespace

bool is_encoded(std::span<const std::uint8_t> data, std::uint32_t expected_magic) noexcept {
    if (data.size() < sizeof(Header)) {
        return false;
    }

    std::uint32_t magic = 0;
    std::memcpy(&magic, data.data(), sizeof(magic));
    return magic == expected_magic;
}

Result<Header> peek_header(std::span<const std::uint8_t> data, const ProjectOptions& project) {
    return parse_header(data, project);
}

Result<std::vector<std::uint8_t>> encode_impl(std::span<const std::uint8_t> raw_buffer,
                                              const ProjectOptions& project,
                                              const AssetOptions& asset) {
    if (const ErrorCode size_error = validate_size(raw_buffer.size()); size_error != ErrorCode::Success) {
        return make_error<std::vector<std::uint8_t>>(size_error);
    }

    Header header{};
    header.magic = project.magic;
    header.domain_id = project.domain_id;
    header.codec_version = Header::CodecVersion;
    header.asset_version = asset.asset_version;
    header.identity_type = static_cast<std::uint8_t>(asset.identity_type);
    header.scramble_id = project.scramble_id;
    header.flags = HeaderFlags::None;
    header.uncompressed_size = static_cast<std::uint32_t>(raw_buffer.size());
    header.compressed_size = static_cast<std::uint32_t>(raw_buffer.size());
    header.masked_crc = mask_crc(compute_crc32(raw_buffer), project);
    header.header_size = static_cast<std::uint16_t>(sizeof(Header) + asset.padding_size);
    header.compression_id = static_cast<std::uint8_t>(CompressionType::None);
    header.nonce = asset.nonce;

    std::vector<std::uint8_t> payload(raw_buffer.begin(), raw_buffer.end());

    if (asset.compress) {
        Result<std::vector<std::uint8_t>> compressed_payload = compress_payload(raw_buffer, CompressionType::Deflate);
        if (!compressed_payload) {
            return make_error<std::vector<std::uint8_t>>(compressed_payload.error);
        }

        payload = std::move(compressed_payload.value);
        set_flag(header.flags, HeaderFlags::Compressed);
        header.compression_id = static_cast<std::uint8_t>(CompressionType::Deflate);
    }

    header.compressed_size = static_cast<std::uint32_t>(payload.size());

    if (asset.scramble) {
        set_flag(header.flags, HeaderFlags::Scrambled);
        if (const ErrorCode error = transform_in_place(payload, project, asset, header); error != ErrorCode::Success) {
            return make_error<std::vector<std::uint8_t>>(error);
        }
    }

    const std::size_t header_size = header.header_size;
    const std::uint64_t padding_seed = make_entropy_seed(raw_buffer.size() ^ payload.size() ^ header.nonce);
    transform_header(header, project);

    std::vector<std::uint8_t> encoded(header_size + payload.size());
    std::memcpy(encoded.data(), &header, sizeof(header));
    if (header_size > sizeof(Header)) {
        fill_with_noise(std::span<std::uint8_t>{encoded}.subspan(sizeof(Header), header_size - sizeof(Header)),
                        padding_seed);
    }
    if (!payload.empty()) {
        std::memcpy(encoded.data() + header_size, payload.data(), payload.size());
    }

    return Result<std::vector<std::uint8_t>>{std::move(encoded)};
}

Result<std::vector<std::uint8_t>> decode_impl(std::span<const std::uint8_t> encoded_buffer,
                                              const ProjectOptions& project,
                                              const AssetOptions& asset) {
    if (!Security::PreDecode(encoded_buffer)) {
        return make_error<std::vector<std::uint8_t>>(ErrorCode::PreFlightAbort);
    }

    const Result<Header> header_result = peek_header(encoded_buffer, project);
    if (!header_result) {
        return make_error<std::vector<std::uint8_t>>(header_result.error);
    }

    std::vector<std::uint8_t> decoded(header_result.value.uncompressed_size);
    const ErrorCode error = decode_into(encoded_buffer, decoded, project, asset);
    if (error != ErrorCode::Success) {
        return make_error<std::vector<std::uint8_t>>(error);
    }

    return Result<std::vector<std::uint8_t>>{std::move(decoded)};
}

Result<std::string> decode_to_string(std::span<const std::uint8_t> encoded_buffer,
                                     const ProjectOptions& project,
                                     const AssetOptions& asset) {
    Result<std::vector<std::uint8_t>> decoded = decode_impl(encoded_buffer, project, asset);
    if (!decoded) {
        return make_error<std::string>(decoded.error);
    }

    std::string text(reinterpret_cast<const char*>(decoded.value.data()), decoded.value.size());
    SecureWipe(decoded.value);
    return Result<std::string>{std::move(text)};
}

ErrorCode encode_file(const std::filesystem::path& input_path,
                      const std::filesystem::path& output_path,
                      const ProjectOptions& project,
                      const AssetOptions& asset) {
    Result<std::vector<std::uint8_t>> input = read_file_bytes(input_path);
    if (!input) {
        return input.error;
    }

    Result<std::vector<std::uint8_t>> encoded = encode_impl(input.value, project, asset);
    SecureWipe(input.value);
    if (!encoded) {
        return encoded.error;
    }

    return write_file_bytes(output_path, encoded.value);
}

ErrorCode decode_file(const std::filesystem::path& input_path,
                      const std::filesystem::path& output_path,
                      const ProjectOptions& project,
                      const AssetOptions& asset) {
    Result<std::vector<std::uint8_t>> input = read_file_bytes(input_path);
    if (!input) {
        return input.error;
    }

    Result<std::vector<std::uint8_t>> decoded = decode_impl(input.value, project, asset);
    SecureWipe(input.value);
    if (!decoded) {
        return decoded.error;
    }

    const ErrorCode write_error = write_file_bytes(output_path, decoded.value);
    SecureWipe(decoded.value);
    return write_error;
}

ErrorCode decode_into(std::span<const std::uint8_t> encoded_buffer,
                      std::span<std::uint8_t> output,
                      const ProjectOptions& project,
                      const AssetOptions& asset) {
    const Result<Header> header_result = peek_header(encoded_buffer, project);
    if (!header_result) {
        return header_result.error;
    }

    const Header& header = header_result.value;
    if (header.domain_id != project.domain_id) {
        return report_tamper_error(ErrorCode::DomainMismatch);
    }

    const std::span<const std::uint8_t> payload = encoded_buffer.subspan(header.header_size);
    return decode_payload_into(payload, header, output, project, asset);
}

} // namespace ripstop::codec
