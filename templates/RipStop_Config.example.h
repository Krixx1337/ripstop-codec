#pragma once

#include <cstdint>
#include <memory>
#include <string_view>

// 1. Error Output

// Optional: switch RipStop to numeric-only error output in release-style builds.
// #define RIPSTOP_ERROR_XOR 0x12345678u
// #define RIPSTOP_HARDEN_ERRORS 1

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

// You can either edit the constants in this file directly, or generate a randomized
// project-local config with:
//   python tools/generate_config.py
//
// Copy this file into your application codebase, rename it to RipStop_Config.h,
// and replace all placeholder values with project-owned constants.
//
// Rotation guidance:
// - Changing ripstopDomainId invalidates every previously protected asset for this project.
// - Changing kProjectSecret invalidates every previously protected asset.
// - Changing any tag* constant invalidates all assets written with that tag.
// - Changing kDefaultAssetVersion should be treated as a caller-driven cache/version bump.
//
// In short: domain, secret, and tags are compatibility boundaries. Rotate them intentionally.

// 2. Asset Identity & Policy

// Project/domain identifier.
// MUST be replaced for your project.
inline constexpr std::uint32_t ripstopDomainId = 0x52505354u; // Example: 'RPST'

// Project-specific file marker.
// MUST be replaced for your project. This can mimic another container or be any private value.
inline constexpr std::uint32_t ripstopMagic = 0x54535052u; // Example: 'RPST'

// Caller-owned payload version policy.
inline constexpr std::uint16_t kDefaultAssetVersion = 1u;

// Asset-class tags.
// MUST be replaced with project-owned values.
inline constexpr std::uint64_t tagPrimaryAsset = 0x1111111122222222ull;
inline constexpr std::uint64_t tagSecondaryAsset = 0x3333333344444444ull;

// 3. Project Secret

// The compiler stores the masked bytes. `.resolve()` unmasks the 64-bit secret at runtime.
inline constexpr auto kProjectSecret =
    ripstop::codec::utils::make_obfuscated_secret<0x123456789ABCDEF0ull, 0x5Cu>();

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
