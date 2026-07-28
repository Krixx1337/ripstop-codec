#include <ripstop/Codec.h>

#include <cstdint>
#include <span>
#include <vector>

int main() {
    constexpr auto identity = ripstop::codec::GenerateIdentity("package-smoke");
    const ripstop::codec::ProjectOptions project{
        .magic = identity.magic,
        .domain_id = identity.domain_id,
        .project_secret = identity.project_secret,
    };
    const std::vector<std::uint8_t> input{1, 2, 3};
    const auto encoded = ripstop::codec::encode(std::span{input}, project);
    const auto decoded = encoded ? ripstop::codec::decode(*encoded, project)
                                 : ripstop::codec::Result<std::vector<std::uint8_t>>{
                                       .error = encoded.error};
    return decoded && decoded.value == input ? 0 : 1;
}
