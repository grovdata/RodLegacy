#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <cstdint>

namespace fs = std::filesystem;

std::vector<uint8_t> ZipExtractToMemory(const fs::path& zipPath, const std::string& entryName);

bool ZipContainsFile(const fs::path& zipPath, const std::string& entryName);
