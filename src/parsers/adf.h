#ifndef ADF_H
#define ADF_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h> // for size_t

// Forward declarations
struct AdfDevice;
struct AdfVolume;
struct AdfFile;

// Define the struct for file data
typedef struct AdfFileData {
    unsigned char* data;
    size_t size;
} AdfFileData;

// Function declarations
struct AdfDevice* adf_mount_device(const char* filename, int readonly);
void adf_unmount_device(struct AdfDevice* dev);
struct AdfVolume* adf_mount_volume(struct AdfDevice* dev, int vol_num, int readonly);
void adf_unmount_volume(struct AdfVolume* vol);

// File extraction functions
struct AdfFileData* extract_adf_file_to_memory(const char *adf_path, const char *file_to_extract);
void free_adf_file_data(struct AdfFileData* data);

#ifdef __cplusplus
}
#endif

#endif // ADF_H
