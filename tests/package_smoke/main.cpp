#include <ripstop/Codec.h>

#include <cstdint>
#include <span>
#include <vector>

int main() {
    constexpr auto project = ripstop::codec::make_project_options("package-smoke");
    const std::vector<std::uint8_t> input{1, 2, 3};
    const auto encoded = ripstop::codec::encode(std::span{input}, project);
    const auto decoded = encoded ? ripstop::codec::decode(*encoded, project)
                                 : ripstop::codec::Result<std::vector<std::uint8_t>>{
                                       .error = encoded.error};
    return decoded && decoded.value == input ? 0 : 1;
}
