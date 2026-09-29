#ifndef ADF_H
#define ADF_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

struct AdfDevice;
struct AdfVolume;
struct AdfFile;

typedef struct AdfFileData {
    unsigned char* data;
    size_t size;
} AdfFileData;

typedef struct AdfFileNameList {
    char** names;
    size_t count;
} AdfFileNameList;

struct AdfDevice* adf_mount_device(const char* filename, int readonly);
void adf_unmount_device(struct AdfDevice* dev);
struct AdfVolume* adf_mount_volume(struct AdfDevice* dev, int vol_num, int readonly);
void adf_unmount_volume(struct AdfVolume* vol);

// Root-directory files ending in `suffix` (case-insensitive, NULL lists all). NULL on mount failure.
struct AdfFileNameList* list_adf_files_to_memory(const char *adf_path, const char *suffix);
struct AdfFileNameList* list_adf_files_from_buffer(const unsigned char *adf_data, size_t adf_size,
                                                    const char *suffix);
void free_adf_file_name_list(struct AdfFileNameList* list);

struct AdfFileData* extract_adf_file_to_memory(const char *adf_path, const char *file_to_extract);

// Extract a file from an ADF image held in memory (uses a temp file internally).
struct AdfFileData* extract_adf_file_from_buffer(const unsigned char *adf_data, size_t adf_size,
                                                  const char *file_to_extract);

void free_adf_file_data(struct AdfFileData* data);

#ifdef __cplusplus
}
#endif

#endif
