#pragma once

#include <cstdint>

namespace exepack {

struct FarPtr {
    std::uint16_t segment{};
    std::uint16_t offset{};
};

inline std::uint32_t linear(FarPtr p) {
    return static_cast<std::uint32_t>(p.segment) * 16u + p.offset;
}

} // namespace exepack
