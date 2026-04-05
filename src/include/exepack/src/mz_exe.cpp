#include "exepack/mz_exe.hpp"

#include <cstring>

namespace exepack {

namespace {

constexpr std::uint16_t kMagic = 0x5a4d;
constexpr std::uint16_t kHeaderLen = 28;

std::uint16_t read_le16(const std::uint8_t* p) {
    std::uint16_t v;
    std::memcpy(&v, p, 2);
    return v;
}

void write_le16(std::uint8_t* p, std::uint16_t v) { std::memcpy(p, &v, 2); }

std::size_t pages_512(std::size_t n) {
    if (n % 512 == 0) {
        return n / 512;
    }
    return n / 512 + 1;
}

bool decode_exe_len(std::uint16_t e_cblp, std::uint16_t e_cp, std::uint64_t* out_len) {
    const std::uint64_t cp = e_cp;
    const std::uint64_t cblp = e_cblp;
    if (e_cblp == 0) {
        *out_len = cp * 512;
        return true;
    }
    if (e_cp == 0) {
        return false;
    }
    if (e_cblp > 511) {
        return false;
    }
    *out_len = (cp - 1) * 512 + cblp;
    return true;
}

bool encode_exe_len(std::size_t len, std::uint16_t* e_cblp, std::uint16_t* e_cp) {
    const std::size_t pg = pages_512(len);
    if (pg > 0xffff) {
        return false;
    }
    *e_cp = static_cast<std::uint16_t>(pg);
    *e_cblp = static_cast<std::uint16_t>(len % 512);
    return true;
}

struct Checksum {
    std::uint16_t sum = 0;
    bool odd = false;

    void update(const std::uint8_t* data, std::size_t len) {
        if (len == 0) {
            return;
        }
        std::size_t i = 0;
        if (odd) {
            sum = static_cast<std::uint16_t>(sum + (static_cast<std::uint16_t>(data[0]) << 8));
            odd = false;
            i = 1;
        }
        for (; i + 1 < len; i += 2) {
            std::uint16_t w;
            std::memcpy(&w, data + i, 2);
            sum = static_cast<std::uint16_t>(sum + w);
        }
        if (i < len) {
            sum = static_cast<std::uint16_t>(sum + data[i]);
            odd = true;
        }
    }

    std::uint16_t finalize() const { return static_cast<std::uint16_t>(~sum); }
};

} // namespace

std::optional<MzExe> MzExe::read_bytes(const std::uint8_t* input, std::size_t len,
                                      std::string* err_message) {
    auto fail = [&](const char* msg) -> std::optional<MzExe> {
        if (err_message) {
            *err_message = msg;
        }
        return std::nullopt;
    };

    if (len < kHeaderLen) {
        return fail("EXE too short for header");
    }
    if (read_le16(input) != kMagic) {
        return fail("bad EXE magic");
    }
    const std::uint16_t e_cblp = read_le16(input + 2);
    const std::uint16_t e_cp = read_le16(input + 4);
    std::uint64_t exe_len_u64 = 0;
    if (!decode_exe_len(e_cblp, e_cp, &exe_len_u64)) {
        return fail("invalid EXE size encoding");
    }
    const std::uint16_t e_crlc = read_le16(input + 6);
    const std::uint16_t e_cparhdr = read_le16(input + 8);
    const std::uint64_t header_len = static_cast<std::uint64_t>(e_cparhdr) * 16;

    MzExe exe{};
    exe.e_minalloc = read_le16(input + 10);
    exe.e_maxalloc = read_le16(input + 12);
    exe.e_ss = read_le16(input + 14);
    exe.e_sp = read_le16(input + 16);
    /* e_csum @ 18 skipped */
    exe.e_ip = read_le16(input + 20);
    exe.e_cs = read_le16(input + 22);
    const std::uint16_t e_lfarlc = read_le16(input + 24);
    exe.e_ovno = read_le16(input + 26);

    std::uint64_t pos = kHeaderLen;
    const std::uint64_t relocs_offset = e_lfarlc;

    if (e_crlc > 0) {
        if (relocs_offset < pos) {
            return fail("relocation table overlaps header");
        }
        if (relocs_offset > len) {
            return fail("unexpected EOF before relocations");
        }
        pos += (relocs_offset - pos);
        if (pos + static_cast<std::uint64_t>(e_crlc) * 4 > len) {
            return fail("unexpected EOF reading relocations");
        }
        exe.relocs.reserve(e_crlc);
        for (std::uint32_t r = 0; r < e_crlc; ++r) {
            FarPtr p{};
            p.offset = read_le16(input + pos);
            pos += 2;
            p.segment = read_le16(input + pos);
            pos += 2;
            exe.relocs.push_back(p);
        }
    }

    if (header_len < pos) {
        return fail("EXE header too short");
    }
    if (header_len > len) {
        return fail("unexpected EOF before end of header");
    }
    pos += (header_len - pos);

    if (exe_len_u64 < pos) {
        return fail("EXE too short to contain header");
    }
    const std::size_t body_len = static_cast<std::size_t>(exe_len_u64 - pos);
    if (pos > len || len - pos < body_len) {
        return fail("unexpected EOF reading EXE body");
    }
    exe.body.assign(input + pos, input + pos + body_len);
    return exe;
}

bool MzExe::write_bytes(std::vector<std::uint8_t>& w, std::string* err_message) const {
    auto fail = [&](const char* msg) -> bool {
        if (err_message) {
            *err_message = msg;
        }
        return false;
    };

    if (relocs.size() > 0xffff) {
        return fail("too many relocations");
    }

    const std::size_t reloc_bytes = relocs.size() * 4;
    const std::size_t fixed_and_relocs = kHeaderLen + reloc_bytes;
    const std::size_t num_header_pages = pages_512(fixed_and_relocs);
    const std::size_t padded_header_len = num_header_pages * 512;
    const std::size_t exe_len = padded_header_len + body.size();

    std::uint16_t e_cblp = 0;
    std::uint16_t e_cp = 0;
    if (!encode_exe_len(exe_len, &e_cblp, &e_cp)) {
        return fail("EXE size too large to represent");
    }

    const std::uint16_t e_crlc = static_cast<std::uint16_t>(relocs.size());
    const std::uint16_t e_cparhdr =
        static_cast<std::uint16_t>((num_header_pages * 512) / 16);

    w.clear();
    w.resize(padded_header_len + body.size());

    std::uint8_t* hdr = w.data();
    write_le16(hdr + 0, kMagic);
    write_le16(hdr + 2, e_cblp);
    write_le16(hdr + 4, e_cp);
    write_le16(hdr + 6, e_crlc);
    write_le16(hdr + 8, e_cparhdr);
    write_le16(hdr + 10, e_minalloc);
    write_le16(hdr + 12, e_maxalloc);
    write_le16(hdr + 14, e_ss);
    write_le16(hdr + 16, e_sp);
    write_le16(hdr + 18, 0); /* e_csum placeholder */
    write_le16(hdr + 20, e_ip);
    write_le16(hdr + 22, e_cs);
    write_le16(hdr + 24, kHeaderLen);
    write_le16(hdr + 26, e_ovno);

    std::size_t p = kHeaderLen;
    for (const FarPtr& fp : relocs) {
        write_le16(hdr + p, fp.offset);
        p += 2;
        write_le16(hdr + p, fp.segment);
        p += 2;
    }
    while (p < padded_header_len) {
        hdr[p++] = 0;
    }
    std::memcpy(w.data() + padded_header_len, body.data(), body.size());

    Checksum c;
    c.update(w.data(), padded_header_len);
    c.update(body.data(), body.size());
    const std::uint16_t e_csum = c.finalize();
    write_le16(w.data() + 18, e_csum);

    return true;
}

} // namespace exepack
