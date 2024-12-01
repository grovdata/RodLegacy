#include "adflib.h"
#include "adf.h"

struct AdfDevice* adf_mount_device(const char* filename, int readonly) {
    return adfMountDev(filename, readonly);
}

void adf_unmount_device(struct AdfDevice* dev) {
    adfUnMountDev(dev);
}

struct AdfVolume* adf_mount_volume(struct AdfDevice* dev, int vol_num, int readonly) {
    return adfMount(dev, vol_num, readonly);
}

void adf_unmount_volume(struct AdfVolume* vol) {
    adfUnMount(vol);
}

struct AdfFileData* extract_adf_file_to_memory(const char *adf_path, const char *file_to_extract) {
    struct AdfDevice *dev = NULL;
    struct AdfVolume *vol = NULL;
    struct AdfFile *file = NULL;
    struct AdfFileData* result = NULL;
    unsigned char* buffer = NULL;
    size_t total_size = 0;
    size_t current_pos = 0;

    // Initialize ADFlib
    adfEnvInitDefault();

    // Mount device
    if (!(dev = adfMountDev(adf_path, TRUE))) {
        fprintf(stderr, "Cannot mount ADF file: %s\n", adf_path);
        goto cleanup;
    }

    // Mount volume (assuming volume 0)
    if (!(vol = adfMount(dev, 0, TRUE))) {
        fprintf(stderr, "Cannot mount volume\n");
        goto cleanup;
    }

    // Open file from ADF
    if (!(file = adfFileOpen(vol, file_to_extract, ADF_FILE_MODE_READ))) {
        fprintf(stderr, "Cannot find file '%s' in ADF\n", file_to_extract);
        goto cleanup;
    }

    // Get file size
    total_size = file->fileHdr->byteSize;
    
    // Allocate memory for the file data
    buffer = (unsigned char*)malloc(total_size);
    if (!buffer) {
        fprintf(stderr, "Memory allocation failed\n");
        goto cleanup;
    }

    // Read entire file into memory
    current_pos = 0;
    while (!adfEndOfFile(file)) {
        unsigned n = adfFileRead(file, total_size - current_pos, buffer + current_pos);
        if (n == 0) break;
        current_pos += n;
    }

    if (current_pos == total_size) {
        // Success - prepare return structure
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
    if (vol) adfUnMount(vol);
    if (dev) adfUnMountDev(dev);
    adfEnvCleanUp();

    return result;
}

// Don't forget to free the returned data when done
void free_adf_file_data(AdfFileData* data) {
    if (data) {
        free(data->data);
        free(data);
    }
}