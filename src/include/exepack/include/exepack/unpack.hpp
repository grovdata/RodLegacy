#pragma once

#include "exepack/mz_exe.hpp"

#include <string>
#include <vector>

namespace exepack {

struct UnpackExeResult {
    std::vector<std::uint8_t> exe_bytes;
    std::string error;

    bool ok() const { return error.empty(); }
};

UnpackExeResult unpack_exepack_to_exe_bytes(const std::uint8_t* packed, std::size_t len);

inline UnpackExeResult unpack_exepack_to_exe_bytes(const std::vector<std::uint8_t>& packed) {
    return unpack_exepack_to_exe_bytes(packed.data(), packed.size());
}

} // namespace exepack
