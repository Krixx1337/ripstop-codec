#pragma once

#include <cstdint>
#include <memory>
#include <string_view>

// 1. Error Output

// RipStop hardens error strings by default.
// #define RIPSTOP_ERROR_XOR 0x12345678u
// #define RIPSTOP_LEAK_STRINGS_FOR_DEBUGGING 1

#include <ripstop/Codec.h>

namespace ripstop_config {

// 2. Security Lifecycle Hooks

class ExampleSecurityPolicy final : public ripstop::codec::ISecurityPolicy {
public:
    bool PreDecode(std::span<const std::uint8_t> encodedData) const override {
        (void)(encodedData);
        return true;
    }

    void OnScrambleState(std::uint64_t& state) const override {
        (void)(state);
    }

    bool PostDescramble(std::span<std::uint8_t> decodedBuffer) const override {
        (void)(decodedBuffer);
        return true;
    }

    void OnTamper(ripstop::codec::ErrorCode code) const override {
        (void)(code);
    }

    void OnError(ripstop::codec::ErrorCode code) const override {
        (void)(code);
    }
};

// Dev No Think path:
// 1. Copy this file into your application codebase.
// 2. Rename it to RipStop_Config.h.
// 3. Change kProjectSeed to a project-unique string.
//
// Rotation guidance:
// - Changing ripstopDomainId invalidates every previously protected asset for this project.
// - Changing kProjectSecret invalidates every previously protected asset.
// - Changing any tag* constant invalidates all assets written with that tag.
// - Changing kDefaultAssetVersion should be treated as a caller-driven cache/version bump.
//
// In short: domain, secret, and tags are compatibility boundaries. Rotate them intentionally.

// 2. Asset Identity & Policy

inline constexpr std::string_view kProjectSeed = "ChangeMe_To_Something_Unique";
inline constexpr auto kProjectIdentity = ripstop::codec::GenerateIdentity(kProjectSeed);

// Project/domain identifier and file marker are derived from the seed string.
inline constexpr std::uint32_t ripstopDomainId = kProjectIdentity.domain_id;
inline constexpr std::uint32_t ripstopMagic = kProjectIdentity.magic;

// Caller-owned payload version policy.
inline constexpr std::uint16_t kDefaultAssetVersion = 1u;

// Asset-class tags are also derived from the same seed to avoid copy-pasted defaults.
inline constexpr std::uint64_t tagPrimaryAsset = ripstop::codec::utils::hash_string(kProjectSeed, "tag:primary");
inline constexpr std::uint64_t tagSecondaryAsset = ripstop::codec::utils::hash_string(kProjectSeed, "tag:secondary");

// 3. Project Secret

// The compiler stores the masked bytes. `.resolve()` unmasks the 64-bit secret at runtime.
inline constexpr auto kProjectSecret =
    ripstop::codec::utils::make_obfuscated_secret<kProjectIdentity.project_secret, 0x5Cu>();

// Optional deterministic helpers for caller-owned context derivation.
inline constexpr std::uint64_t HashContextString(std::string_view value) {
    return ripstop::codec::utils::hash_string(value);
}

inline constexpr std::uint64_t HashContextUint64(std::uint64_t value) {
    return ripstop::codec::utils::hash_uint64(value);
}

// Recommended default policy:
// - choose one tag per asset class
// - derive context_seed from a stable logical asset identifier
// - leave nonce at 0 unless you explicitly want non-deterministic output
inline constexpr std::uint64_t ExampleTextureContextSeed =
    HashContextString("textures/player_idle");

// Advanced: this is where you define your project's unique "digital thumbprint".
// No two projects should use the same math here.
inline void my_custom_scrambler(std::span<std::uint8_t> buffer,
                                std::uint64_t state,
                                const ripstop::codec::Header& header) {
    for (std::uint8_t& byte : buffer) {
        state = ripstop::codec::detail::mix64(state + 0x9e3779b97f4a7c15ull +
                                              static_cast<std::uint64_t>(header.asset_version));
        const std::uint8_t rotate = static_cast<std::uint8_t>((state ^ header.identity_type) & 0x07u);
        const std::uint8_t scrambled =
            static_cast<std::uint8_t>((byte << rotate) | (byte >> ((8u - rotate) & 0x07u)));
        byte = static_cast<std::uint8_t>(scrambled ^ ((state >> 24) & 0xFFu));
    }
}

inline ripstop::codec::ProjectOptions MakeProjectOptions() {
    return {
        .magic = ripstopMagic,
        .domain_id = ripstopDomainId,
        .project_secret = kProjectSecret.resolve(),
        .scramble_id = 100,
        .scrambler = my_custom_scrambler,
        .policy = std::make_shared<ExampleSecurityPolicy>(),
    };
}

inline constexpr ripstop::codec::AssetOptions MakeAssetOptions(
    std::uint64_t formatTag,
    std::uint64_t contextSeed = 0,
    std::uint64_t nonce = 0,
    std::uint16_t assetVersion = kDefaultAssetVersion,
    ripstop::codec::IdentityType identityType = ripstop::codec::IdentityType::None,
    std::uint8_t paddingSize = 0,
    bool compress = true,
    bool scramble = true) {
    // Leave nonce at 0 for deterministic builds. Set it to a timestamp or random value
    // when you want each encode to produce a different binary blob.
    return {
        .format_tag = formatTag,
        .context_seed = contextSeed,
        .nonce = nonce,
        .asset_version = assetVersion,
        .identity_type = identityType,
        .padding_size = paddingSize,
        .compress = compress,
        .scramble = scramble,
    };
}

} // namespace ripstop_config
