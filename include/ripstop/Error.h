#pragma once

#include <ripstop/config_bridge.h>
#include <ripstop/detail/polymorphic_error.h>

#include <cassert>
#include <cstdint>
#include <string>
#include <utility>

namespace ripstop::codec {

#if !RIPSTOP_HAS_CUSTOM_ERROR_CODE_ENUM
enum class [[nodiscard]] ErrorCode : std::uint32_t {
    Success = 0,
    BufferTooSmall,
    MagicMismatch,
    UnsupportedVersion,
    UnsupportedScrambleId,
    MissingScramblerFunc,
    InvalidFlags,
    InvalidIdentityType,
    SizeLimitExceeded,
    DomainMismatch,
    UnsupportedCompression,
    CompressionFailed,
    DecompressionFailed,
    CrcMismatch,
    FileOpenFailed,
    FileReadFailed,
    FileWriteFailed
};
#endif

[[nodiscard]] inline std::string to_string(ErrorCode error) {
#if RIPSTOP_HARDEN_ERRORS
    return ::ripstop::hostile_core::harden_error_code<ErrorCode, static_cast<std::uint32_t>(RIPSTOP_ERROR_XOR)>(error);
#else
    switch (error) {
    case ErrorCode::Success:
        return "Success";
    case ErrorCode::BufferTooSmall:
        return "BufferTooSmall";
    case ErrorCode::MagicMismatch:
        return "MagicMismatch";
    case ErrorCode::UnsupportedVersion:
        return "UnsupportedVersion";
    case ErrorCode::UnsupportedScrambleId:
        return "UnsupportedScrambleId";
    case ErrorCode::MissingScramblerFunc:
        return "MissingScramblerFunc";
    case ErrorCode::InvalidFlags:
        return "InvalidFlags";
    case ErrorCode::InvalidIdentityType:
        return "InvalidIdentityType";
    case ErrorCode::SizeLimitExceeded:
        return "SizeLimitExceeded";
    case ErrorCode::DomainMismatch:
        return "DomainMismatch";
    case ErrorCode::UnsupportedCompression:
        return "UnsupportedCompression";
    case ErrorCode::CompressionFailed:
        return "CompressionFailed";
    case ErrorCode::DecompressionFailed:
        return "DecompressionFailed";
    case ErrorCode::CrcMismatch:
        return "CrcMismatch";
    case ErrorCode::FileOpenFailed:
        return "FileOpenFailed";
    case ErrorCode::FileReadFailed:
        return "FileReadFailed";
    case ErrorCode::FileWriteFailed:
        return "FileWriteFailed";
    }

    return "UnknownError";
#endif
}

template <typename T>
struct [[nodiscard]] Result {
    T value{};
    ErrorCode error = ErrorCode::Success;

    explicit operator bool() const noexcept {
        return error == ErrorCode::Success;
    }

    [[nodiscard]] bool has_value() const noexcept {
        return static_cast<bool>(*this);
    }

    [[nodiscard]] T& operator*() & noexcept {
        assert(has_value() && "Attempted to dereference an error Result");
        return value;
    }

    [[nodiscard]] const T& operator*() const & noexcept {
        assert(has_value() && "Attempted to dereference an error Result");
        return value;
    }

    [[nodiscard]] T* operator->() noexcept {
        assert(has_value() && "Attempted to dereference an error Result");
        return &value;
    }

    [[nodiscard]] const T* operator->() const noexcept {
        assert(has_value() && "Attempted to dereference an error Result");
        return &value;
    }

    template <typename U>
    [[nodiscard]] T value_or(U&& default_value) const & {
        if (has_value()) {
            return value;
        }

        return static_cast<T>(std::forward<U>(default_value));
    }
};

} // namespace ripstop::codec
