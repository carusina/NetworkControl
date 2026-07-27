#pragma once

#include <cstdint>
#include <WinSock2.h>

namespace Common {

    inline uint64_t ByteSwap64(uint64_t v) {
        return ((v & 0x00000000000000FFull) << 56) |
               ((v & 0x000000000000FF00ull) << 40) |
               ((v & 0x0000000000FF0000ull) << 24) |
               ((v & 0x00000000FF000000ull) << 8)  |
               ((v & 0x000000FF00000000ull) >> 8)  |
               ((v & 0x0000FF0000000000ull) >> 24) |
               ((v & 0x00FF000000000000ull) >> 40) |
               ((v & 0xFF00000000000000ull) >> 56);
    }

    inline bool IsLittleEndian() {
        const uint16_t x = 1;
        return *reinterpret_cast<const uint8_t*>(&x) == 1;
    }

    inline uint64_t HostToNetwork64(uint64_t v) {
        return IsLittleEndian() ? ByteSwap64(v) : v;
    }

    inline uint64_t NetworkToHost64(uint64_t v) {
        return IsLittleEndian() ? ByteSwap64(v) : v;
    }

} // namespace Common