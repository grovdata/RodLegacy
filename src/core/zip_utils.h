/*
 * RodLegacy - Reads entries from zip files into memory.
 *
 * Created: 2026-04-14
 * SPDX-License-Identifier: GPL-3.0-or-later (see LICENSE)
 */

#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <cstdint>

namespace fs = std::filesystem;

std::vector<uint8_t> ZipExtractToMemory(const fs::path& zipPath, const std::string& entryName);

bool ZipContainsFile(const fs::path& zipPath, const std::string& entryName);
