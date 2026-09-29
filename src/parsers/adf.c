// ADFlib's headers warn in our translation unit; not our code.
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wimplicit-int-conversion"
#endif
#include "adflib.h"
#ifdef __clang__
#pragma clang diagnostic pop
#endif
#include "parsers/adf.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#ifndef _WIN32
#include <unistd.h>
#include <strings.h>
#else
#include <windows.h>
#define strcasecmp _stricmp
#endif

struct AdfDevice* adf_mount_device(const char* filename, int readonly) {
    return adfDevOpen(filename, readonly ? ADF_ACCESS_MODE_READONLY : ADF_ACCESS_MODE_READWRITE);
}

void adf_unmount_device(struct AdfDevice* dev) {
    adfDevClose(dev);
}

struct AdfVolume* adf_mount_volume(struct AdfDevice* dev, int vol_num, int readonly) {
    return adfVolMount(dev, vol_num, readonly ? ADF_ACCESS_MODE_READONLY : ADF_ACCESS_MODE_READWRITE);
}

void adf_unmount_volume(struct AdfVolume* vol) {
    adfVolUnMount(vol);
}

struct AdfFileData* extract_adf_file_to_memory(const char *adf_path, const char *file_to_extract) {
    struct AdfDevice *dev = NULL;
    struct AdfVolume *vol = NULL;
    struct AdfFile *file = NULL;
    struct AdfFileData* result = NULL;
    unsigned char* buffer = NULL;
    size_t total_size = 0;
    size_t current_pos = 0;

    adfLibInit();
    adfEnvSetProperty(ADF_PR_QUIET, true);

    dev = adfDevOpen(adf_path, ADF_ACCESS_MODE_READONLY);
    if (!dev) {
        fprintf(stderr, "Cannot open ADF file: %s\n", adf_path);
        goto cleanup;
    }

    ADF_RETCODE rc = adfDevMount(dev);
    if (rc != ADF_RC_OK) {
        fprintf(stderr, "Cannot mount ADF device: %s\n", adf_path);
        goto cleanup;
    }

    vol = adfVolMount(dev, 0, ADF_ACCESS_MODE_READONLY);
    if (!vol) {
        fprintf(stderr, "Cannot mount volume from ADF: %s\n", adf_path);
        goto cleanup;
    }

    if (!(file = adfFileOpen(vol, file_to_extract, ADF_FILE_MODE_READ))) {
        fprintf(stderr, "Cannot find file '%s' in ADF\n", file_to_extract);
        goto cleanup;
    }

    total_size = file->fileHdr->byteSize;
    
    buffer = (unsigned char*)malloc(total_size);
    if (!buffer) {
        fprintf(stderr, "Memory allocation failed\n");
        goto cleanup;
    }

    current_pos = 0;
    while (!adfFileAtEOF(file)) {
        unsigned n = adfFileRead(file, total_size - current_pos, buffer + current_pos);
        if (n == 0) break;
        current_pos += n;
    }

    if (current_pos == total_size) {
        result = (struct AdfFileData*)malloc(sizeof(AdfFileData));
        if (result) {
            result->data = buffer;
            result->size = total_size;
            buffer = NULL; // Prevent cleanup from freeing the buffer
        }
    }

cleanup:
    if (buffer) free(buffer);
    if (file) adfFileClose(file);
    if (vol) adfVolUnMount(vol);
    if (dev) {
        adfDevUnMount(dev);
        adfDevClose(dev);
    }
    adfLibCleanUp();

    return result;
}

struct AdfFileData* extract_adf_file_from_buffer(const unsigned char *adf_data, size_t adf_size,
                                                  const char *file_to_extract) {
    // ADFlib needs a file path, so go through a temp file.
    char tempPath[1024];
#ifdef _WIN32
    char tempDir[512] = "";
    GetTempPathA(sizeof(tempDir), tempDir);
    if (!GetTempFileNameA(tempDir, "srl", 0, tempPath)) {
        fprintf(stderr, "extract_adf_file_from_buffer: cannot create temp file\n");
        return NULL;
    }
    FILE *f = fopen(tempPath, "wb");
    if (!f) {
        fprintf(stderr, "extract_adf_file_from_buffer: cannot open temp file %s\n", tempPath);
        return NULL;
    }
#else
    snprintf(tempPath, sizeof(tempPath), "/tmp/rodlegacy_adf_tmp_XXXXXX");
    int fd = mkstemp(tempPath);
    if (fd == -1) {
        fprintf(stderr, "extract_adf_file_from_buffer: cannot create temp file\n");
        return NULL;
    }
    FILE *f = fdopen(fd, "wb");
    if (!f) {
        fprintf(stderr, "extract_adf_file_from_buffer: cannot open temp file %s\n", tempPath);
        close(fd);
        remove(tempPath);
        return NULL;
    }
#endif
    fwrite(adf_data, 1, adf_size, f);
    fclose(f);

    struct AdfFileData *result = extract_adf_file_to_memory(tempPath, file_to_extract);

    remove(tempPath);
    return result;
}

static int has_suffix_ci(const char *name, const char *suffix) {
    if (!suffix || !*suffix) return 1;
    size_t nameLen = strlen(name);
    size_t sufLen = strlen(suffix);
    if (sufLen > nameLen) return 0;
    return strcasecmp(name + (nameLen - sufLen), suffix) == 0;
}

struct AdfFileNameList* list_adf_files_to_memory(const char *adf_path, const char *suffix) {
    struct AdfDevice *dev = NULL;
    struct AdfVolume *vol = NULL;
    struct AdfList *entryList = NULL;
    struct AdfFileNameList *result = NULL;
    char **names = NULL;
    size_t capacity = 16;
    size_t count = 0;

    adfLibInit();
    adfEnvSetProperty(ADF_PR_QUIET, true);

    dev = adfDevOpen(adf_path, ADF_ACCESS_MODE_READONLY);
    if (!dev) {
        fprintf(stderr, "Cannot open ADF file: %s\n", adf_path);
        goto cleanup;
    }
    if (adfDevMount(dev) != ADF_RC_OK) {
        fprintf(stderr, "Cannot mount ADF device: %s\n", adf_path);
        goto cleanup;
    }
    vol = adfVolMount(dev, 0, ADF_ACCESS_MODE_READONLY);
    if (!vol) {
        fprintf(stderr, "Cannot mount volume from ADF: %s\n", adf_path);
        goto cleanup;
    }
    if (adfToRootDir(vol) != ADF_RC_OK) {
        fprintf(stderr, "Cannot read root directory of ADF: %s\n", adf_path);
        goto cleanup;
    }

    names = (char**)malloc(capacity * sizeof(char*));
    if (!names) goto cleanup;

    entryList = adfGetDirEnt(vol, vol->curDirPtr);
    for (struct AdfList *cell = entryList; cell != NULL; cell = cell->next) {
        struct AdfEntry *entry = (struct AdfEntry*)cell->content;
        if (!entry || entry->type != ADF_ST_FILE) continue;
        if (!has_suffix_ci(entry->name, suffix)) continue;
        if (count == capacity) {
            capacity *= 2;
            char **grown = (char**)realloc(names, capacity * sizeof(char*));
            if (!grown) goto cleanup;
            names = grown;
        }
        names[count] = strdup(entry->name);
        if (!names[count]) goto cleanup;
        ++count;
    }

    result = (struct AdfFileNameList*)malloc(sizeof(AdfFileNameList));
    if (result) {
        result->names = names;
        result->count = count;
        names = NULL; // ownership transferred
    }

cleanup:
    if (entryList) adfFreeDirList(entryList);
    if (names) {
        for (size_t i = 0; i < count; ++i) free(names[i]);
        free(names);
    }
    if (vol) adfVolUnMount(vol);
    if (dev) {
        adfDevUnMount(dev);
        adfDevClose(dev);
    }
    adfLibCleanUp();

    return result;
}

struct AdfFileNameList* list_adf_files_from_buffer(const unsigned char *adf_data, size_t adf_size,
                                                    const char *suffix) {
    char tempPath[1024];
#ifdef _WIN32
    char tempDir[512] = "";
    GetTempPathA(sizeof(tempDir), tempDir);
    if (!GetTempFileNameA(tempDir, "srl", 0, tempPath)) {
        fprintf(stderr, "list_adf_files_from_buffer: cannot create temp file\n");
        return NULL;
    }
    FILE *f = fopen(tempPath, "wb");
    if (!f) {
        fprintf(stderr, "list_adf_files_from_buffer: cannot open temp file %s\n", tempPath);
        return NULL;
    }
#else
    snprintf(tempPath, sizeof(tempPath), "/tmp/rodlegacy_adf_tmp_XXXXXX");
    int fd = mkstemp(tempPath);
    if (fd == -1) {
        fprintf(stderr, "list_adf_files_from_buffer: cannot create temp file\n");
        return NULL;
    }
    FILE *f = fdopen(fd, "wb");
    if (!f) {
        fprintf(stderr, "list_adf_files_from_buffer: cannot open temp file %s\n", tempPath);
        close(fd);
        remove(tempPath);
        return NULL;
    }
#endif
    fwrite(adf_data, 1, adf_size, f);
    fclose(f);

    struct AdfFileNameList *result = list_adf_files_to_memory(tempPath, suffix);

    remove(tempPath);
    return result;
}

void free_adf_file_name_list(struct AdfFileNameList* list) {
    if (!list) return;
    for (size_t i = 0; i < list->count; ++i) free(list->names[i]);
    free(list->names);
    free(list);
}

void free_adf_file_data(AdfFileData* data) {
    if (data) {
        free(data->data);
        free(data);
    }
}