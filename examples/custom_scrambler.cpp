#include <ripstop/Codec.h>

#include <cstdint>
#include <span>
#include <vector>

namespace {

void custom_xor_scrambler(std::span<std::uint8_t> buffer,
                          std::uint64_t state,
                          const ripstop::codec::Header& header) {
    state ^= ripstop::codec::utils::hash_uint64(header.asset_version);
    for (std::uint8_t& byte : buffer) {
        state += ripstop::codec::detail::split_mix_increment;
        byte ^= static_cast<std::uint8_t>(ripstop::codec::detail::mix64(state));
    }
}

} // namespace

int main() {
    constexpr auto identity = ripstop::codec::GenerateIdentity("advanced-custom-scrambler-example");
    const ripstop::codec::ProjectOptions project{
        .magic = identity.magic,
        .domain_id = identity.domain_id,
        .project_secret = identity.project_secret,
        .scramble_id = 42, // Stable, project-owned, and non-zero.
        .scrambler = &custom_xor_scrambler,
    };

    const std::vector<std::uint8_t> input{1, 2, 3, 4};
    const auto encoded = ripstop::codec::encode(std::span{input}, project);
    if (!encoded) {
        return 1;
    }

    const auto decoded = ripstop::codec::decode(*encoded, project);
    return decoded && decoded.value == input ? 0 : 1;
}
