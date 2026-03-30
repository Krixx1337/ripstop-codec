#pragma once

#include <ripstop/config_bridge.h>
#include <ripstop/detail/polymorphic_error.h>

#include <cassert>
#include <cstdint>
#include <string>
#include <utility>

namespace ripstop::codec {

namespace detail {

std::uint32_t ErrorXorKey() noexcept;

} // namespace detail

enum class ErrorCode : std::uint32_t {
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
    PreFlightAbort,
    FileOpenFailed,
    FileReadFailed,
    FileWriteFailed
};

[[nodiscard]] std::string to_string(ErrorCode error);

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
