#include <ripstop/Codec.h>

#include <cstddef>
#include <cstdint>
#include <span>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    constexpr auto project = ripstop::codec::make_project_options("ripstop-fuzz");
    const std::span<const std::uint8_t> input{data, size};
    (void)ripstop::codec::peek_header(input, project);
    (void)ripstop::codec::decode(input, project);
    return 0;
}
