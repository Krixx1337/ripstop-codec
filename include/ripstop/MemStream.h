#pragma once

#include <cstddef>
#include <cstdint>
#include <istream>
#include <limits>
#include <span>
#include <streambuf>

namespace ripstop::codec {

class MemBuf final : public std::streambuf {
public:
    MemBuf(const std::uint8_t* data, std::size_t size) {
        if (data != nullptr && size != 0) {
            m_begin = reinterpret_cast<char*>(const_cast<std::uint8_t*>(data));
            m_end = m_begin + size;
        } else {
            m_begin = &m_empty;
            m_end = &m_empty;
        }
        setg(m_begin, m_begin, m_end);
    }

protected:
    pos_type seekoff(off_type off, std::ios_base::seekdir dir, std::ios_base::openmode which) override {
        if ((which & std::ios_base::in) == 0) {
            return pos_type(off_type(-1));
        }

        const std::ptrdiff_t currentOffset = gptr() - m_begin;
        const std::ptrdiff_t size = m_end - m_begin;
        std::ptrdiff_t baseOffset = 0;
        if (dir == std::ios_base::beg) {
            baseOffset = 0;
        } else if (dir == std::ios_base::cur) {
            baseOffset = currentOffset;
        } else if (dir == std::ios_base::end) {
            baseOffset = size;
        } else {
            return pos_type(off_type(-1));
        }

        if (off > 0 && baseOffset > (std::numeric_limits<std::ptrdiff_t>::max)() - off) {
            return pos_type(off_type(-1));
        }
        if (off < 0 && baseOffset < (std::numeric_limits<std::ptrdiff_t>::min)() - off) {
            return pos_type(off_type(-1));
        }

        const std::ptrdiff_t newOffset = baseOffset + static_cast<std::ptrdiff_t>(off);
        if (newOffset < 0 || newOffset > size) {
            return pos_type(off_type(-1));
        }

        char* newPos = m_begin + newOffset;
        setg(m_begin, newPos, m_end);
        return pos_type(newOffset);
    }

    pos_type seekpos(pos_type pos, std::ios_base::openmode which) override {
        return seekoff(static_cast<off_type>(pos), std::ios_base::beg, which);
    }

private:
    char* m_begin = nullptr;
    char* m_end = nullptr;
    char m_empty = 0;
};

class MemStream final : public std::istream {
public:
    MemStream(const std::uint8_t* data, std::size_t size)
        : std::istream(&m_buffer), m_buffer(data, size) {
    }

    explicit MemStream(std::span<const std::uint8_t> data)
        : MemStream(data.data(), data.size()) {
    }

private:
    MemBuf m_buffer;
};

} // namespace ripstop::codec
