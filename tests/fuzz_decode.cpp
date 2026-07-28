#include <ripstop/Codec.h>

#include <cstddef>
#include <cstdint>
#include <span>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    constexpr auto identity = ripstop::codec::GenerateIdentity("ripstop-fuzz");
    const ripstop::codec::ProjectOptions project{
        .magic = identity.magic,
        .domain_id = identity.domain_id,
        .project_secret = identity.project_secret,
    };
    const std::span<const std::uint8_t> input{data, size};
    (void)ripstop::codec::peek_header(input, project);
    (void)ripstop::codec::decode(input, project);
    return 0;
}
