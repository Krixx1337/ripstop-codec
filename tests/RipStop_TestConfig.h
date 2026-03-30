#pragma once

#include <cstdint>
#include <span>

#define RIPSTOP_HARDEN_ERRORS 1

namespace ripstop::codec {
struct TestSecurityPolicy {
    static inline bool PreDecode(std::span<const std::uint8_t>) {
        return true;
    }

    static inline void OnScrambleState(std::uint64_t&) {}

    static inline bool PostDescramble(std::span<std::uint8_t>) {
        return true;
    }

    static inline void OnTamper(ErrorCode) {}

    static inline void OnError(ErrorCode) {}
};

} // namespace ripstop::codec

#define RIPSTOP_SECURITY_POLICY ::ripstop::codec::TestSecurityPolicy
