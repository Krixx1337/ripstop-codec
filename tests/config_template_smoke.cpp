#include <RipStop_Config.example.h>

#include <cstdint>
#include <span>
#include <vector>

int main() {
    const auto project = ripstop_config::MakeProjectOptions();
    const auto asset = ripstop_config::MakeAssetOptions(
        ripstop_config::tagPrimaryAsset,
        ripstop_config::HashContextString("config-template-smoke"));
    const std::vector<std::uint8_t> input{1, 2, 3, 4};
    const auto encoded = ripstop::codec::encode(std::span{input}, project, asset);
    const auto decoded = encoded ? ripstop::codec::decode(*encoded, project, asset)
                                 : ripstop::codec::Result<std::vector<std::uint8_t>>{
                                       .error = encoded.error};
    return decoded && decoded.value == input ? 0 : 1;
}
