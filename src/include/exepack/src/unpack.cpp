#include "exepack/unpack.hpp"

#include <algorithm>
#include <cstring>
#include <optional>

namespace exepack {

namespace {

constexpr std::uint16_t kExepackSignature = 0x4252; // "RB"

std::uint16_t read_le16(const std::uint8_t* p) {
    std::uint16_t v;
    std::memcpy(&v, p, 2);
    return v;
}

std::size_t paras(std::size_t n) {
    if (n % 16 == 0) {
        return n / 16;
    }
    return n / 16 + 1;
}

std::size_t unpad(const std::vector<std::uint8_t>& buf, std::size_t i) {
    std::size_t take = 0;
    std::size_t k = i;
    while (take < 15 && k > 0 && buf[k - 1] == 0xff) {
        ++take;
        --k;
    }
    return i - take;
}

struct ExepackHeader {
    std::uint16_t real_ip{};
    std::uint16_t real_cs{};
    std::uint16_t exepack_size{};
    std::uint16_t real_sp{};
    std::uint16_t real_ss{};
    std::uint16_t dest_len{};
    std::uint16_t skip_len{1};
};

bool parse_exepack_header(const std::uint8_t* buf, std::size_t len, ExepackHeader* out,
                         std::string* err) {
    if (len != 16 && len != 18) {
        if (err) {
            *err = "unknown EXEPACK header length";
        }
        return false;
    }
    out->real_ip = read_le16(buf);
    out->real_cs = read_le16(buf + 2);
    /* mem_start 4..6 ignored */
    out->exepack_size = read_le16(buf + 6);
    out->real_sp = read_le16(buf + 8);
    out->real_ss = read_le16(buf + 10);
    out->dest_len = read_le16(buf + 12);
    std::size_t sig_off = 0;
    if (len == 18) {
        out->skip_len = read_le16(buf + 14);
        sig_off = 16;
    } else {
        out->skip_len = 1;
        sig_off = 14;
    }
    const std::uint16_t sig = read_le16(buf + sig_off);
    if (sig != kExepackSignature) {
        if (err) {
            *err = "bad EXEPACK signature";
        }
        return false;
    }
    return true;
}

const std::uint8_t kStubSuffix[] = {0xcd, 0x21, 0xb8, 0xff, 0x4c, 0xcd, 0x21};
constexpr std::size_t kStubMessageLen = 22;

std::optional<std::size_t> locate_end_of_stub(const std::uint8_t* stub, std::size_t stub_len) {
    if (stub_len < sizeof(kStubSuffix) + kStubMessageLen) {
        return std::nullopt;
    }
    for (std::size_t i = 0; i + sizeof(kStubSuffix) <= stub_len; ++i) {
        if (std::memcmp(stub + i, kStubSuffix, sizeof(kStubSuffix)) == 0) {
            return i + sizeof(kStubSuffix) + kStubMessageLen;
        }
    }
    return std::nullopt;
}

bool parse_packed_relocs(const std::uint8_t* buf, std::size_t len, std::vector<FarPtr>* relocs,
                        std::string* err) {
    relocs->clear();
    std::size_t i = 0;
    for (unsigned seg = 0; seg < 16; ++seg) {
        if (i + 2 > len) {
            if (err) {
                *err = "EXEPACK relocation table truncated";
            }
            return false;
        }
        const std::uint16_t num = read_le16(buf + i);
        i += 2;
        for (std::uint32_t j = 0; j < num; ++j) {
            if (i + 2 > len) {
                if (err) {
                    *err = "EXEPACK relocation table truncated";
                }
                return false;
            }
            const std::uint16_t off = read_le16(buf + i);
            i += 2;
            FarPtr p{};
            p.segment = static_cast<std::uint16_t>(seg * 0x1000);
            p.offset = off;
            relocs->push_back(p);
        }
    }
    if (i != len) {
        if (err) {
            *err = "EXEPACK stub/relocs size mismatch";
        }
        return false;
    }
    return true;
}

bool decompress(std::vector<std::uint8_t>& buf, std::size_t compressed_len,
               std::size_t uncompressed_len, std::string* err) {
    std::size_t src = compressed_len;
    std::size_t dst = uncompressed_len;

    if (dst > buf.size()) {
        buf.resize(dst);
    }

    src = unpad(buf, src);

    for (;;) {
        if (src == 0) {
            if (err) {
                *err = "EXEPACK read overflow";
            }
            return false;
        }
        --src;
        const std::uint8_t command = buf[src];
        if (src < 2) {
            if (err) {
                *err = "EXEPACK read overflow";
            }
            return false;
        }
        src -= 2;
        const std::uint16_t length_u16 = read_le16(buf.data() + src);
        const std::size_t length = length_u16;

        const std::uint8_t cmd = static_cast<std::uint8_t>(command & 0xfe);
        if (cmd == 0xb0) {
            if (src == 0) {
                if (err) {
                    *err = "EXEPACK read overflow";
                }
                return false;
            }
            --src;
            const std::uint8_t fill = buf[src];
            if (dst < length) {
                if (err) {
                    *err = "EXEPACK fill overflow";
                }
                return false;
            }
            dst -= length;
            std::fill(buf.begin() + static_cast<std::ptrdiff_t>(dst),
                      buf.begin() + static_cast<std::ptrdiff_t>(dst + length), fill);
        } else if (cmd == 0xb2) {
            if (src < length) {
                if (err) {
                    *err = "EXEPACK read overflow";
                }
                return false;
            }
            src -= length;
            if (dst < length) {
                if (err) {
                    *err = "EXEPACK copy overflow";
                }
                return false;
            }
            dst -= length;
            for (std::size_t i = 0; i < length; ++i) {
                buf[dst + length - 1 - i] = buf[src + length - 1 - i];
            }
        } else {
            if (err) {
                *err = "unknown EXEPACK command";
            }
            return false;
        }

        if ((command & 0x01) != 0) {
            break;
        }
    }

    if (compressed_len < dst) {
        if (err) {
            *err = "EXEPACK decompression gap";
        }
        return false;
    }
    buf.resize(uncompressed_len);
    return true;
}

} // namespace

UnpackExeResult unpack_exepack_to_exe_bytes(const std::uint8_t* packed, std::size_t len) {
    std::string read_err;
    std::optional<MzExe> opt_in = MzExe::read_bytes(packed, len, &read_err);
    if (!opt_in) {
        return {{}, std::move(read_err)};
    }
    MzExe exe_in = std::move(*opt_in);

    if (!exe_in.relocs.empty()) {
        return {{}, "relocations before decompression are not supported"};
    }

    const std::size_t packed_body_len = exe_in.body.size();
    std::vector<std::uint8_t> body = std::move(exe_in.body);
    const std::size_t exepack_header_offset = static_cast<std::size_t>(exe_in.e_cs) * 16;
    const std::size_t exepack_header_len = static_cast<std::size_t>(exe_in.e_ip);

    if (exepack_header_offset > body.size()) {
        return {{}, "EXEPACK header past end of file"};
    }
    if (exepack_header_len > body.size() - exepack_header_offset) {
        return {{}, "EXEPACK header past end of file"};
    }

    std::vector<std::uint8_t> exepack_header(body.begin() + static_cast<std::ptrdiff_t>(exepack_header_offset),
                                            body.begin() +
                                                static_cast<std::ptrdiff_t>(exepack_header_offset +
                                                                            exepack_header_len));
    std::vector<std::uint8_t> stub(body.begin() + static_cast<std::ptrdiff_t>(exepack_header_offset +
                                                                              exepack_header_len),
                                  body.end());

    body.erase(body.begin() + static_cast<std::ptrdiff_t>(exepack_header_offset), body.end());

    std::string hdr_err;
    ExepackHeader eh{};
    if (!parse_exepack_header(exepack_header.data(), exepack_header.size(), &eh, &hdr_err)) {
        return {{}, hdr_err};
    }

    const std::size_t hdr_sz = exepack_header.size();
    if (eh.exepack_size < hdr_sz) {
        return {{}, "EXEPACK block too short"};
    }
    const std::size_t stub_region = static_cast<std::size_t>(eh.exepack_size) - hdr_sz;
    if (stub_region > stub.size()) {
        return {{}, "EXEPACK block too short"};
    }
    stub.resize(stub_region);

    const auto stub_end = locate_end_of_stub(stub.data(), stub.size());
    if (!stub_end) {
        return {{}, "unknown EXEPACK decompression stub"};
    }
    const std::size_t stub_len = *stub_end;
    if (stub_len > stub.size()) {
        return {{}, "unknown EXEPACK decompression stub"};
    }

    std::vector<std::uint8_t> relocs_buf(stub.begin() + static_cast<std::ptrdiff_t>(stub_len),
                                        stub.end());

    std::vector<FarPtr> out_relocs;
    std::string rel_err;
    if (!parse_packed_relocs(relocs_buf.data(), relocs_buf.size(), &out_relocs, &rel_err)) {
        return {{}, rel_err};
    }

    if (eh.skip_len == 0) {
        return {{}, "invalid EXEPACK skip_len"};
    }
    // skip_len is 1 greater than the number of 16-byte padding paragraphs
    // between the compressed data and the EXEPACK header (0 means 1 less
    // than minimum, hence the check above). Correct formula: (skip_len-1)*16.
    const std::size_t skip_bytes = (static_cast<std::size_t>(eh.skip_len) - 1) * 16;
    if (skip_bytes > body.size()) {
        return {{}, "invalid EXEPACK skip_len"};
    }
    if (skip_bytes > static_cast<std::size_t>(eh.dest_len) * 16) {
        return {{}, "invalid EXEPACK skip_len"};
    }
    const std::size_t compressed_len = body.size() - skip_bytes;
    const std::size_t uncompressed_len =
        static_cast<std::size_t>(eh.dest_len) * 16 - skip_bytes;

    std::string dec_err;
    if (!decompress(body, compressed_len, uncompressed_len, &dec_err)) {
        return {{}, dec_err};
    }

    /* (paras(packed_body_len) + e_minalloc) - paras(unpacked body), saturating */
    const std::uint32_t in_paras_body = static_cast<std::uint32_t>(paras(packed_body_len));
    const std::uint32_t out_paras_body = static_cast<std::uint32_t>(paras(body.size()));
    std::uint32_t minalloc = in_paras_body + exe_in.e_minalloc;
    if (minalloc < out_paras_body) {
        minalloc = 0;
    } else {
        minalloc -= out_paras_body;
    }
    if (minalloc > 0xffff) {
        return {{}, "e_minalloc too large"};
    }

    MzExe out{};
    out.e_minalloc = static_cast<std::uint16_t>(minalloc);
    out.e_maxalloc = exe_in.e_maxalloc;
    out.e_ss = eh.real_ss;
    out.e_sp = eh.real_sp;
    out.e_ip = eh.real_ip;
    out.e_cs = eh.real_cs;
    out.e_ovno = exe_in.e_ovno;
    out.body = std::move(body);
    out.relocs = std::move(out_relocs);

    std::vector<std::uint8_t> out_bytes;
    std::string werr;
    if (!out.write_bytes(out_bytes, &werr)) {
        return {{}, werr};
    }
    return {std::move(out_bytes), {}};
}

} // namespace exepack
