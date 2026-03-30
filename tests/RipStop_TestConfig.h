#pragma once

#include <cstdint>
#include <memory>
#include <span>

#define RIPSTOP_HARDEN_ERRORS 1

namespace ripstop::codec {
class TestSecurityPolicy final : public ISecurityPolicy {
public:
    bool PreDecode(std::span<const std::uint8_t>) const override {
        return true;
    }

    void OnScrambleState(std::uint64_t&) const override {}

    bool PostDescramble(std::span<std::uint8_t>) const override {
        return true;
    }

    void OnTamper(ErrorCode) const override {}

    void OnError(ErrorCode) const override {}
};

} // namespace ripstop::codec
