#pragma once

#include <cstdlib>
#include <memory>
#include <string>
#include <span>
#include <cstdint>

namespace ripstop::codec {
enum class ErrorCode : std::uint32_t;

class ISecurityPolicy {
public:
    virtual ~ISecurityPolicy() = default;
    virtual bool PreDecode(std::span<const std::uint8_t> encoded_data) const {
        (void)encoded_data;
        return true;
    }
    virtual void OnScrambleState(std::uint64_t& state) const {
        (void)state;
    }
    virtual bool PostDescramble(std::span<std::uint8_t> decoded_buffer) const {
        (void)decoded_buffer;
        return true;
    }
    virtual void OnTamper(ErrorCode code) const = 0;
    virtual void OnError(ErrorCode code) const {
        (void)code;
    }
};
} // namespace ripstop::codec

// Optional project-local config injection, similar to BurnerNet.
#ifdef RIPSTOP_USER_CONFIG_HEADER
#include RIPSTOP_USER_CONFIG_HEADER
#endif

#ifndef RIPSTOP_ERROR_XOR
#define RIPSTOP_ERROR_XOR 0u
#endif

#ifndef RIPSTOP_HARDEN_ERRORS
#define RIPSTOP_HARDEN_ERRORS 0
#endif

namespace ripstop::codec::detail {

class DefaultSecurityPolicy final : public ISecurityPolicy {
public:
    bool PreDecode(std::span<const std::uint8_t>) const override {
        return true;
    }

    void OnScrambleState(std::uint64_t&) const override {}

    bool PostDescramble(std::span<std::uint8_t>) const override {
        return true;
    }

    void OnTamper(ErrorCode code) const override {
        (void)(code);
        std::abort();
    }

    void OnError(ErrorCode code) const override {
        (void)(code);
    }
};

std::shared_ptr<ISecurityPolicy> ResolveSecurityPolicy(std::shared_ptr<ISecurityPolicy> policy);

} // namespace ripstop::codec::detail
