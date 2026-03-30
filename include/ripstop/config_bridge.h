#pragma once

#include <cstdlib>
#include <string>
#include <span>
#include <cstdint>

namespace ripstop::codec {
enum class ErrorCode : std::uint32_t;
} // namespace ripstop::codec

// Optional project-local config injection, similar to BurnerNet.
#ifdef RIPSTOP_USER_CONFIG_HEADER
#include RIPSTOP_USER_CONFIG_HEADER
#endif

#ifndef RIPSTOP_HARDEN_ERRORS
#define RIPSTOP_HARDEN_ERRORS 0
#endif

namespace ripstop::codec::detail {

struct DefaultSecurity {
    static inline bool PreDecode(std::span<const std::uint8_t>) {
        return true;
    }

    static inline void OnScrambleState(std::uint64_t&) {}

    static inline bool PostDescramble(std::span<std::uint8_t>) {
        return true;
    }

    static inline void OnTamper(ErrorCode code) {
        (void)(code);
        std::abort();
    }

    static inline void OnError(ErrorCode code) {
        (void)(code);
    }
};

} // namespace ripstop::codec::detail

#ifndef RIPSTOP_SECURITY_POLICY
namespace ripstop::codec {
using Security = detail::DefaultSecurity;
}
#else
namespace ripstop::codec {
using Security = RIPSTOP_SECURITY_POLICY;
}
#endif
