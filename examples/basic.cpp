#include <ripstop/Codec.h>

#include <iostream>
#include <span>
#include <vector>

int main() {
    constexpr auto project =
        ripstop::codec::make_project_options("example:replace-with-your-project");

    const std::vector<float> input{1.0f, 2.0f, 3.0f};
    const auto encoded = ripstop::codec::encode(std::span{input}, project);
    if (!encoded) {
        std::cerr << ripstop::codec::to_string(encoded.error) << '\n';
        return 1;
    }

    const auto decoded = ripstop::codec::decode_to_vector<float>(*encoded, project);
    if (!decoded) {
        std::cerr << ripstop::codec::to_string(decoded.error) << '\n';
        return 1;
    }

    return decoded.value == input ? 0 : 1;
}
