#include "zip_utils.h"
#include "log.h"

// miniz.h declares zlib-compat shims as static that this file doesn't call;
// not our code to fix warnings in.
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
#endif
extern "C" {
    #include "miniz.h"
}
#ifdef __clang__
#pragma clang diagnostic pop
#endif

std::vector<uint8_t> ZipExtractToMemory(const fs::path& zipPath, const std::string& entryName) {
    mz_zip_archive zip = {};
    if (!mz_zip_reader_init_file(&zip, zipPath.string().c_str(), 0)) {
        LOG_WARN("ZipExtractToMemory: cannot open zip: %s", zipPath.string().c_str());
        return {};
    }

    int idx = mz_zip_reader_locate_file(&zip, entryName.c_str(), nullptr, 0);
    if (idx < 0) {
        LOG_WARN("ZipExtractToMemory: '%s' not found in %s", entryName.c_str(), zipPath.string().c_str());
        mz_zip_reader_end(&zip);
        return {};
    }

    mz_zip_archive_file_stat stat = {};
    if (!mz_zip_reader_file_stat(&zip, idx, &stat)) {
        LOG_WARN("ZipExtractToMemory: stat failed for '%s'", entryName.c_str());
        mz_zip_reader_end(&zip);
        return {};
    }

    std::vector<uint8_t> buf(stat.m_uncomp_size);
    if (!mz_zip_reader_extract_to_mem(&zip, idx, buf.data(), buf.size(), 0)) {
        LOG_WARN("ZipExtractToMemory: extraction failed for '%s'", entryName.c_str());
        mz_zip_reader_end(&zip);
        return {};
    }

    mz_zip_reader_end(&zip);
    return buf;
}

bool ZipContainsFile(const fs::path& zipPath, const std::string& entryName) {
    mz_zip_archive zip = {};
    if (!mz_zip_reader_init_file(&zip, zipPath.string().c_str(), 0)) {
        return false;
    }
    int idx = mz_zip_reader_locate_file(&zip, entryName.c_str(), nullptr, 0);
    mz_zip_reader_end(&zip);
    return idx >= 0;
}
