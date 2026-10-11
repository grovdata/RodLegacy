#include "core/game_text.h"

#include <cctype>

namespace GameText {

namespace {
constexpr size_t kTable = 0x29054;
constexpr size_t kDataHunk = 0x28468;   // the hunk the table's pointers are relative to
constexpr int kCount = 96;

std::vector<std::string> messages;
std::vector<uint8_t> exeCopy;

std::string CString(const std::vector<uint8_t>& d, size_t at) {
    if (at >= d.size()) return {};
    size_t end = at;
    while (end < d.size() && d[end] != 0) ++end;
    return std::string(reinterpret_cast<const char*>(d.data() + at), end - at);
}

uint32_t BeU32(const std::vector<uint8_t>& d, size_t at) {
    return (uint32_t(d[at]) << 24) | (uint32_t(d[at + 1]) << 16) | (uint32_t(d[at + 2]) << 8) | d[at + 3];
}
}

void Load(const std::vector<uint8_t>& exe) {
    messages.assign(kCount, {});
    exeCopy = exe;
    if (exe.size() < kTable + kCount * 10) return;
    for (int i = 0; i < kCount; ++i) {
        const size_t rec = kTable + size_t(i) * 10;
        std::string text;
        for (int l = 0; l < 2; ++l) {
            const uint32_t ptr = BeU32(exe, rec + 2 + size_t(l) * 4);
            if (ptr == 0) break;
            if (l) text += '\n';
            text += CString(exe, kDataHunk + ptr);
        }
        messages[size_t(i)] = text;
    }
}

std::string Msg(Message id) {
    return id >= 0 && id < int(messages.size()) ? messages[size_t(id)] : std::string();
}

std::string ReplaceNumber(const std::string& s, int number) {
    size_t a = 0;
    while (a < s.size() && !std::isdigit(static_cast<unsigned char>(s[a]))) ++a;
    if (a == s.size()) return s;
    size_t b = a;
    while (b < s.size() && std::isdigit(static_cast<unsigned char>(s[b]))) ++b;
    return s.substr(0, a) + std::to_string(number) + s.substr(b);
}

std::string Msg(Message id, int number) { return ReplaceNumber(Msg(id), number); }

std::string Get(Loose at) { return CString(exeCopy, at); }

std::string Get(Loose at, int number) { return ReplaceNumber(Get(at), number); }

}
