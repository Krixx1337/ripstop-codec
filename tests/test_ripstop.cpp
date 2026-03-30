#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <ripstop/Codec.h>
#include <ripstop/MemStream.h>

#include <algorithm>
#include <array>
#include <cstdint>
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
}

TEST_CASE("typed encode and decode preserve POD-like values") {
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

TEST_CASE("error strings are hardened numeric codes when enabled") {
    CHECK(to_string(ErrorCode::MagicMismatch) ==
          std::to_string(static_cast<std::uint32_t>(ErrorCode::MagicMismatch)));
}

TEST_CASE("secure wipe clears caller-owned buffers") {
    std::string text = "secret";
    SecureWipe(text);
    CHECK(text.empty());

    std::vector<std::uint8_t> bytes{1u, 2u, 3u, 4u};
    SecureWipe(bytes);
    CHECK(bytes.empty());
}
