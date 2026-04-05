#pragma once

#include "exepack/far_ptr.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace exepack {

struct MzExe {
    std::uint16_t e_minalloc{};
    std::uint16_t e_maxalloc{};
    std::uint16_t e_ss{};
    std::uint16_t e_sp{};
    std::uint16_t e_ip{};
    std::uint16_t e_cs{};
    std::uint16_t e_ovno{};
    std::vector<FarPtr> relocs;
    std::vector<std::uint8_t> body;

    static std::optional<MzExe> read_bytes(const std::uint8_t* data, std::size_t len,
                                          std::string* err_message);

    static std::optional<MzExe> read_bytes(const std::vector<std::uint8_t>& bytes,
                                          std::string* err_message) {
        return read_bytes(bytes.data(), bytes.size(), err_message);
    }

    bool write_bytes(std::vector<std::uint8_t>& out, std::string* err_message) const;
};

} // namespace exepack
