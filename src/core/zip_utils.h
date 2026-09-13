#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <cstdint>

namespace fs = std::filesystem;

// Extract a named entry from a zip file into a memory buffer.
// Returns empty vector on failure.
std::vector<uint8_t> ZipExtractToMemory(const fs::path& zipPath, const std::string& entryName);

// Check if a zip contains a specific entry (case-sensitive).
bool ZipContainsFile(const fs::path& zipPath, const std::string& entryName);
