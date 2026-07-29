#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <ripstop/Codec.h>
#include <ripstop/MemStream.h>

#include <RipStop_Config.example.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <istream>
#include <span>
#include <string>
#include <vector>

namespace {

using namespace ripstop::codec;

ProjectOptions make_project() {
    return ProjectOptions{
        .magic = 0x474E5089u,
        .domain_id = 0xDEADBEEFu,
        .project_secret = 0x0123456789ABCDEFull,
    };
}

AssetOptions make_asset() {
    return AssetOptions{
        .format_tag = 0x1122334455667788ull,
        .context_seed = 0x8877665544332211ull,
        .nonce = 0x1020304050607080ull,
    };
}

std::vector<std::uint8_t> make_payload(std::size_t size) {
    std::vector<std::uint8_t> data(size);
    std::uint64_t state = 0xA5A55A5ADEADBEEFull;

    for (std::uint8_t& byte : data) {
        state += 0x9e3779b97f4a7c15ull;
        std::uint64_t mixed = state;
        mixed = (mixed ^ (mixed >> 30)) * 0xbf58476d1ce4e5b9ull;
        mixed = (mixed ^ (mixed >> 27)) * 0x94d049bb133111ebull;
        mixed ^= mixed >> 31;
        byte = static_cast<std::uint8_t>(mixed & 0xFFu);
    }

    return data;
}

bool contains_bytes(std::span<const std::uint8_t> haystack, std::span<const std::uint8_t> needle) {
    return std::search(haystack.begin(), haystack.end(), needle.begin(), needle.end()) != haystack.end();
}

void transform_header_for_test(Header& header, const ProjectOptions& project) {
    const std::uint64_t mask = utils::hash_uint64(project.project_secret ^ project.domain_id);
    auto* header_bytes = reinterpret_cast<std::uint8_t*>(&header);

    for (std::size_t i = sizeof(header.magic); i < sizeof(Header); ++i) {
        const std::size_t mask_index = (i - sizeof(header.magic)) % sizeof(mask);
        header_bytes[i] ^= static_cast<std::uint8_t>((mask >> (mask_index * 8)) & 0xFFu);
    }
}

struct PackedPoint {
    std::uint32_t x;
    std::uint32_t y;

    bool operator==(const PackedPoint&) const = default;
};

static_assert(std::has_unique_object_representations_v<PackedPoint>);

constexpr auto k_seed_project = make_project_options("test-project-seed");
constexpr auto k_seed_identity = GenerateIdentity("test-project-seed");
static_assert(k_seed_project.magic == k_seed_identity.magic);
static_assert(k_seed_project.domain_id == k_seed_identity.domain_id);
static_assert(k_seed_project.project_secret == k_seed_identity.project_secret);
static_assert(k_seed_project.scramble_id == Header::ScrambleSplitMix64);
static_assert(k_seed_project.scrambler == nullptr);

} // namespace

TEST_CASE("round-trip integrity across compression and scrambling permutations") {
    const ProjectOptions project = make_project();
    AssetOptions asset = make_asset();
    const std::vector<std::uint8_t> input = make_payload(1024);

    SUBCASE("compress + scramble") {
        asset.compress = true;
        asset.scramble = true;
    }

    SUBCASE("compress only") {
        asset.compress = true;
        asset.scramble = false;
    }

    SUBCASE("scramble only") {
        asset.compress = false;
        asset.scramble = true;
    }

    SUBCASE("raw payload") {
        asset.compress = false;
        asset.scramble = false;
    }

    const auto encoded = encode(std::span{input}, project, asset);
    REQUIRE(encoded);
    CHECK(is_encoded(*encoded, project.magic));

    const auto decoded = decode(*encoded, project, asset);
    REQUIRE(decoded);
    CHECK(decoded.value == input);
}

TEST_CASE("header fields are masked on disk") {
    const ProjectOptions project = make_project();
    AssetOptions asset = make_asset();
    asset.compress = true;
    asset.scramble = true;

    const std::vector<std::uint8_t> input(64, 0);
    const auto encoded = encode(std::span{input}, project, asset);
    REQUIRE(encoded);

    const std::array<std::uint8_t, sizeof(project.domain_id)> domain_bytes{
        static_cast<std::uint8_t>(project.domain_id & 0xFFu),
        static_cast<std::uint8_t>((project.domain_id >> 8) & 0xFFu),
        static_cast<std::uint8_t>((project.domain_id >> 16) & 0xFFu),
        static_cast<std::uint8_t>((project.domain_id >> 24) & 0xFFu),
    };

    CHECK_FALSE(contains_bytes(*encoded, std::span{domain_bytes}));
}

TEST_CASE("tampering and mismatched project settings fail safely") {
    const ProjectOptions project = make_project();
    AssetOptions asset = make_asset();
    asset.compress = false;
    asset.scramble = false;

    const std::vector<std::uint8_t> input = make_payload(128);
    const auto encoded = encode(std::span{input}, project, asset);
    REQUIRE(encoded);

    SUBCASE("payload mutation trips CRC") {
        std::vector<std::uint8_t> tampered = encoded.value;
        tampered[sizeof(Header)] ^= 0x01u;

        const auto decoded = decode(tampered, project, asset);
        CHECK_FALSE(decoded);
        CHECK(decoded.error == ErrorCode::CrcMismatch);
    }

    SUBCASE("wrong magic is rejected") {
        ProjectOptions wrong_project = project;
        wrong_project.magic ^= 0x01010101u;

        const auto decoded = decode(*encoded, wrong_project, asset);
        CHECK_FALSE(decoded);
        CHECK(decoded.error == ErrorCode::MagicMismatch);
    }

    SUBCASE("wrong domain is rejected") {
        const auto header = peek_header(*encoded, project);
        REQUIRE(header);

        std::vector<std::uint8_t> tampered = encoded.value;
        Header mutated = header.value;
        mutated.domain_id ^= 0x0000FFFFu;
        transform_header_for_test(mutated, project);
        std::memcpy(tampered.data(), &mutated, sizeof(mutated));

        const auto decoded = decode(tampered, project, asset);
        CHECK_FALSE(decoded);
        CHECK(decoded.error == ErrorCode::DomainMismatch);
    }
}

TEST_CASE("decode_into supports caller-owned output buffers") {
    const ProjectOptions project = make_project();
    const AssetOptions asset = make_asset();
    const std::vector<std::uint8_t> input = make_payload(257);

    const auto encoded = encode(std::span{input}, project, asset);
    REQUIRE(encoded);

    const auto header = peek_header(*encoded, project);
    REQUIRE(header);

    std::vector<std::uint8_t> output(header->uncompressed_size);
    const ErrorCode error = decode_into(*encoded, std::span{output}, project, asset);

    CHECK(error == ErrorCode::Success);
    CHECK(output == input);

    output.pop_back();
    CHECK(decode_into(*encoded, std::span{output}, project, asset) == ErrorCode::BufferTooSmall);
}

TEST_CASE("typed encode and decode preserve trivially copyable values") {
    const ProjectOptions project = make_project();
    const AssetOptions asset = make_asset();
    const std::vector<PackedPoint> input{
        {10u, 20u},
        {30u, 40u},
        {50u, 60u},
    };

    const auto encoded = encode<PackedPoint>(std::span{input}, project, asset);
    REQUIRE(encoded);

    const auto decoded = decode_to_vector<PackedPoint>(*encoded, project, asset);
    REQUIRE(decoded);
    CHECK(decoded.value == input);
}

TEST_CASE("typed encode supports common floating-point vectors") {
    const ProjectOptions project = make_project();
    const std::vector<float> input{1.0f, 2.5f, -7.25f};

    const auto encoded = encode<float>(std::span{input}, project);
    REQUIRE(encoded);

    const auto decoded = decode_to_vector<float>(*encoded, project);
    REQUIRE(decoded);
    CHECK(decoded.value == input);
}

TEST_CASE("empty payloads still produce a valid container") {
    const ProjectOptions project = make_project();
    AssetOptions asset = make_asset();
    asset.compress = false;
    const std::vector<std::uint8_t> input;

    const auto encoded = encode(std::span{input}, project, asset);
    REQUIRE(encoded);
    CHECK(encoded->size() == sizeof(Header));
    CHECK(is_encoded(*encoded, project.magic));

    const auto header = peek_header(*encoded, project);
    REQUIRE(header);
    CHECK(header->uncompressed_size == 0u);
    CHECK(header->compressed_size == 0u);

    const auto decoded = decode(*encoded, project, asset);
    REQUIRE(decoded);
    CHECK(decoded->empty());
}

TEST_CASE("MemStream provides zero-copy istream-style reads over decoded buffers") {
    const std::string text = "RipStop stream bridge";
    const std::span<const std::uint8_t> bytes{
        reinterpret_cast<const std::uint8_t*>(text.data()),
        text.size(),
    };

    MemStream stream(bytes);

    std::string first;
    std::string second;
    stream >> first >> second;

    CHECK(first == "RipStop");
    CHECK(second == "stream");

    stream.clear();
    stream.seekg(0, std::ios_base::beg);

    std::string whole_line;
    std::getline(stream, whole_line);
    CHECK(whole_line == text);
}

TEST_CASE("error strings are stable and readable") {
    CHECK(to_string(ErrorCode::MagicMismatch) == "MagicMismatch");
    CHECK(to_string(static_cast<ErrorCode>(9999)) == "UnknownError");
}

TEST_CASE("secure wipe clears caller-owned buffers") {
    std::string text = "secret";
    SecureWipe(text);
    CHECK(text.empty());

    std::vector<std::uint8_t> bytes{1u, 2u, 3u, 4u};
    SecureWipe(bytes);
    CHECK(bytes.empty());
}

TEST_CASE("encoding is deterministic with default nonce and no padding") {
    const ProjectOptions project = make_project();
    const AssetOptions asset = make_asset();
    const std::vector<std::uint8_t> input = make_payload(200);

    const auto first = encode(std::span{input}, project, asset);
    const auto second = encode(std::span{input}, project, asset);

    REQUIRE(first);
    REQUIRE(second);
    CHECK(first.value == second.value);
}

TEST_CASE("format-v1 golden asset remains byte-compatible") {
    const ProjectOptions project = make_project();
    const AssetOptions asset = make_asset();
    const std::vector<std::uint8_t> input{
        0x52, 0x69, 0x70, 0x53, 0x74, 0x6f, 0x70, 0x20,
        0x76, 0x31, 0x2e, 0x30, 0x2e, 0x31,
    };
    constexpr std::array<std::uint8_t, 65> golden{
        0x89, 0x50, 0x4e, 0x47, 0xaa, 0xf3, 0x5f, 0x66, 0xfd, 0xa7, 0x39,
        0x4b, 0x45, 0x4d, 0xf1, 0xb8, 0xf2, 0xa7, 0x38, 0x4b, 0x5c, 0x4d,
        0xf2, 0xb8, 0x79, 0x7e, 0xcd, 0xf9, 0x6d, 0x4d, 0xf3, 0xb8, 0x7c,
        0xd7, 0x58, 0x1b, 0x05, 0x7d, 0xd2, 0xa8, 0x53, 0x77, 0xf2, 0x29,
        0xdd, 0x41, 0xbd, 0x6e, 0x1e, 0x02, 0x48, 0x1a, 0xaa, 0xe8, 0xd3,
        0xac, 0x6b, 0x1f, 0x60, 0x35, 0x0d, 0xb0, 0x63, 0xaf, 0xaf,
    };

    const auto encoded = encode(std::span{input}, project, asset);
    REQUIRE(encoded);
    CHECK(std::ranges::equal(encoded.value, golden));

    const auto decoded = decode(std::span{golden}, project, asset);
    REQUIRE(decoded);
    CHECK(decoded.value == input);
}

TEST_CASE("malformed headers fail without callbacks or process termination") {
    const ProjectOptions project = make_project();
    AssetOptions asset = make_asset();
    asset.compress = false;
    asset.scramble = false;
    const std::vector<std::uint8_t> input{1, 2, 3};
    const auto encoded = encode(std::span{input}, project, asset);
    REQUIRE(encoded);

    SUBCASE("truncated") {
        const auto decoded = decode(std::span{encoded->data(), encoded->size() - 1}, project, asset);
        CHECK_FALSE(decoded);
        CHECK(decoded.error == ErrorCode::BufferTooSmall);
    }

    SUBCASE("trailing bytes") {
        auto malformed = encoded.value;
        malformed.push_back(0);
        const auto decoded = decode(malformed, project, asset);
        CHECK_FALSE(decoded);
        CHECK(decoded.error == ErrorCode::BufferTooSmall);
    }

    SUBCASE("reserved field") {
        auto malformed = encoded.value;
        Header header = *peek_header(malformed, project);
        header.reserved = 1;
        transform_header_for_test(header, project);
        std::memcpy(malformed.data(), &header, sizeof(header));
        const auto decoded = decode(malformed, project, asset);
        CHECK_FALSE(decoded);
        CHECK(decoded.error == ErrorCode::InvalidFlags);
    }

    SUBCASE("unknown flags") {
        auto malformed = encoded.value;
        Header header = *peek_header(malformed, project);
        header.flags = static_cast<HeaderFlags>(0x8000);
        transform_header_for_test(header, project);
        std::memcpy(malformed.data(), &header, sizeof(header));
        CHECK(decode(malformed, project, asset).error == ErrorCode::InvalidFlags);
    }

    SUBCASE("unsupported old codec version") {
        auto malformed = encoded.value;
        Header header = *peek_header(malformed, project);
        header.codec_version = 0;
        transform_header_for_test(header, project);
        std::memcpy(malformed.data(), &header, sizeof(header));
        CHECK(decode(malformed, project, asset).error == ErrorCode::UnsupportedVersion);
    }

    SUBCASE("invalid identity type") {
        auto malformed = encoded.value;
        Header header = *peek_header(malformed, project);
        header.identity_type = 5;
        transform_header_for_test(header, project);
        std::memcpy(malformed.data(), &header, sizeof(header));
        CHECK(decode(malformed, project, asset).error == ErrorCode::InvalidIdentityType);
    }

    SUBCASE("compression flag and id mismatch") {
        auto malformed = encoded.value;
        Header header = *peek_header(malformed, project);
        header.flags = HeaderFlags::Compressed;
        header.compression_id = static_cast<std::uint8_t>(CompressionType::None);
        transform_header_for_test(header, project);
        std::memcpy(malformed.data(), &header, sizeof(header));
        CHECK(decode(malformed, project, asset).error == ErrorCode::UnsupportedCompression);
    }

    SUBCASE("declared payload exceeds limit") {
        auto malformed = encoded.value;
        Header header = *peek_header(malformed, project);
        header.uncompressed_size = 256u * 1024u * 1024u + 1u;
        transform_header_for_test(header, project);
        std::memcpy(malformed.data(), &header, sizeof(header));
        CHECK(decode(malformed, project, asset).error == ErrorCode::SizeLimitExceeded);
    }
}

TEST_CASE("custom scramblers are deterministic and round-trip") {
    auto xor_scrambler = +[](std::span<std::uint8_t> buffer, std::uint64_t state, const Header&) {
        for (std::uint8_t& byte : buffer) {
            state += detail::split_mix_increment;
            byte ^= static_cast<std::uint8_t>(detail::mix64(state));
        }
    };

    ProjectOptions project = make_project();
    project.scrambler = xor_scrambler;
    project.scramble_id = 23;
    const std::vector<std::uint8_t> input = make_payload(64);

    const auto encoded = encode(std::span{input}, project);
    const auto encoded_again = encode(std::span{input}, project);
    REQUIRE(encoded);
    REQUIRE(encoded_again);
    CHECK(encoded.value == encoded_again.value);
    const auto decoded = decode(*encoded, project);
    REQUIRE(decoded);
    CHECK(decoded.value == input);

    project.scramble_id = Header::ScrambleSplitMix64;
    const auto legacy_encoded = encode(std::span{input}, project);
    REQUIRE(legacy_encoded);
    const auto legacy_decoded = decode(*legacy_encoded, project);
    REQUIRE(legacy_decoded);
    CHECK(legacy_decoded.value == input);
}

TEST_CASE("asset context and padding round-trip predictably") {
    const ProjectOptions project = make_project();
    AssetOptions asset = make_asset();
    asset.padding_size = 31;
    const std::vector<std::uint8_t> input = make_payload(100);

    const auto encoded = encode(std::span{input}, project, asset);
    REQUIRE(encoded);
    const auto header = peek_header(*encoded, project);
    REQUIRE(header);
    CHECK(header->header_size == sizeof(Header) + 31);

    AssetOptions wrong_asset = asset;
    wrong_asset.context_seed ^= 1;
    const ErrorCode wrong_context_error = decode(*encoded, project, wrong_asset).error;
    CHECK((wrong_context_error == ErrorCode::DecompressionFailed ||
           wrong_context_error == ErrorCode::CrcMismatch));

    const auto decoded = decode(*encoded, project, asset);
    REQUIRE(decoded);
    CHECK(decoded.value == input);
}

TEST_CASE("config template round-trips with default helpers") {
    const auto project = ripstop_config::MakeProjectOptions();
    const auto asset = ripstop_config::MakeAssetOptions(
        ripstop_config::tagPrimaryAsset,
        ripstop_config::HashContextString("config-template-smoke"));
    const std::vector<std::uint8_t> input{1, 2, 3, 4};

    const auto encoded = encode(std::span{input}, project, asset);
    REQUIRE(encoded);
    const auto decoded = decode(*encoded, project, asset);
    REQUIRE(decoded);
    CHECK(decoded.value == input);
}

TEST_CASE("file helpers round-trip through transactional replacement") {
    const ProjectOptions project = make_project();
    const auto temp_dir = std::filesystem::temp_directory_path() / "ripstop-codec-tests";
    std::filesystem::create_directories(temp_dir);
    const auto input_path = temp_dir / "input.bin";
    const auto encoded_path = temp_dir / "output.rip";
    const auto decoded_path = temp_dir / "decoded.bin";
    const std::vector<std::uint8_t> input = make_payload(128);

    {
        std::ofstream output(input_path, std::ios::binary);
        output.write(reinterpret_cast<const char*>(input.data()), static_cast<std::streamsize>(input.size()));
    }

    REQUIRE(encode_file(input_path, encoded_path, project) == ErrorCode::Success);
    REQUIRE(decode_file(encoded_path, decoded_path, project) == ErrorCode::Success);

#if !defined(_WIN32)
    constexpr auto private_permissions =
        std::filesystem::perms::owner_read | std::filesystem::perms::owner_write;
    std::error_code permissions_error;
    std::filesystem::permissions(
        encoded_path,
        private_permissions,
        std::filesystem::perm_options::replace,
        permissions_error);
    REQUIRE_FALSE(permissions_error);
    std::filesystem::permissions(
        decoded_path,
        private_permissions,
        std::filesystem::perm_options::replace,
        permissions_error);
    REQUIRE_FALSE(permissions_error);

    REQUIRE(encode_file(input_path, encoded_path, project) == ErrorCode::Success);
    REQUIRE(decode_file(encoded_path, decoded_path, project) == ErrorCode::Success);
    CHECK(std::filesystem::status(encoded_path).permissions() == private_permissions);
    CHECK(std::filesystem::status(decoded_path).permissions() == private_permissions);
#endif

    std::ifstream decoded_file(decoded_path, std::ios::binary);
    const std::vector<std::uint8_t> decoded{
        std::istreambuf_iterator<char>{decoded_file},
        std::istreambuf_iterator<char>{}};
    CHECK(decoded == input);

    std::error_code ignored;
    std::filesystem::remove_all(temp_dir, ignored);
}
