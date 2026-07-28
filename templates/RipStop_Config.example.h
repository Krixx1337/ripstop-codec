#pragma once

#include <ripstop/Codec.h>

#include <cstdint>
#include <string_view>

namespace ripstop_config {

// Change this once, before shipping assets. Changing it later invalidates existing assets.
inline constexpr std::string_view kProjectSeed = "change-me:company/product";
inline constexpr auto kProjectIdentity = ripstop::codec::GenerateIdentity(kProjectSeed);

inline constexpr std::uint16_t kDefaultAssetVersion = 1u;
inline constexpr std::uint64_t tagPrimaryAsset =
    ripstop::codec::utils::hash_string(kProjectSeed, "tag:primary");
inline constexpr std::uint64_t tagSecondaryAsset =
    ripstop::codec::utils::hash_string(kProjectSeed, "tag:secondary");

inline constexpr auto kProjectSecret =
    ripstop::codec::utils::make_obfuscated_secret<kProjectIdentity.project_secret, 0x5Cu>();

[[nodiscard]] inline ripstop::codec::ProjectOptions MakeProjectOptions() {
    return {
        .magic = kProjectIdentity.magic,
        .domain_id = kProjectIdentity.domain_id,
        .project_secret = kProjectSecret.resolve(),
    };
}

[[nodiscard]] inline constexpr std::uint64_t HashContextString(std::string_view value) {
    return ripstop::codec::utils::hash_string(value);
}

[[nodiscard]] inline constexpr std::uint64_t HashContextUint64(std::uint64_t value) {
    return ripstop::codec::utils::hash_uint64(value);
}

[[nodiscard]] inline constexpr ripstop::codec::AssetOptions MakeAssetOptions(
    std::uint64_t formatTag,
    std::uint64_t contextSeed = 0,
    std::uint64_t nonce = 0,
    std::uint16_t assetVersion = kDefaultAssetVersion,
    ripstop::codec::IdentityType identityType = ripstop::codec::IdentityType::None,
    std::uint8_t paddingSize = 0,
    bool compress = true,
    bool scramble = true) {
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
