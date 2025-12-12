/* Mark Pfister
 * CSC 310 - Operating Systems Final Project
 * 12/11/2025
 * recover_files.c
 *
 * A utility to recover deleted files from a QFS filesystem image.
 *
 * Usage: recover_files <filesystem_image>
 *
 * This program opens the specified QFS filesystem image, scans for deleted files,
 * and attempts to recover them by reading their data blocks and writing them to
 * the local filesystem.
*/

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "qfs.h"

#define DATA_START_OFFSET 8192 // Superblock of 32 bytes plus directory entries of 8160 bytes

int main(int argc, char *argv[]) {

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <filesystem_image>\n", argv[0]);
        return 1;
    }

    FILE *fp = fopen(argv[1], "rb");
    if (!fp) {
        perror("fopen");
        return 2;
    }

#ifdef DEBUG
    printf("Opened disk image: %s\n", argv[1]);
#endif

    // Read the filesystem superblock structure from the beginning of the image.
    // `superblock_t` is defined in `qfs.h` and holds metadata such as the
    // filesystem type, bytes-per-block and number of blocks.
    // A successful read ensures we can interpret the rest of the image.
    superblock_t superblock;
    if (fread(&superblock, sizeof(superblock_t), 1, fp) != 1) {
        fprintf(stderr, "Error: failed to read superblock.\n");
        fclose(fp);
        return 3;
    }

    // Confirm this is a valid QFS filesystem
    if (superblock.fs_type != 0x51) {
        fprintf(stderr, "Error: not a valid QFS image.\n");
        fclose(fp);
        return 4;
    }

    // Extract block size and number of blocks from the superblock. These
    // determine how many bytes of data we need to read for the entire data
    // region: `data_size = bytes_per_block * total_blocks`.
    uint16_t block_size = superblock.bytes_per_block;
    uint16_t total_blocks = superblock.total_blocks;
    uint32_t data_size = (uint32_t)block_size * total_blocks;

    // Allocate a buffer to hold all data blocks. This simplifies scanning
    // because we can treat the data region as one contiguous byte array.
    uint8_t *buffer = malloc(data_size);
    if (!buffer) {
        fprintf(stderr, "Error: memory allocation failed.\n");
        fclose(fp);
        return 5;
    }

    // Move the file pointer to the start of the data blocks region. The
    // superblock and directory area precede this region.
    fseek(fp, DATA_START_OFFSET, SEEK_SET);

    // Read raw block data
    if (fread(buffer, 1, data_size, fp) != data_size) {
        fprintf(stderr, "Error: failed to read data blocks.\n");
        free(buffer);
        fclose(fp);
        return 6;
    }

    fclose(fp); // No longer need the disk image file

    // Scan for JPG signatures
    const uint8_t JPG_START[2] = {0xFF, 0xD8};
    const uint8_t JPG_END[2] = {0xFF, 0xD9};

    int writing = 0;       // Whether we are currently writing a discovered JPG
    int file_count = 0;    // Number of recovered JPGs found so far
    FILE *out_fp = NULL;   // Output file pointer for the current JPG

    // Scan the data region byte-by-byte. We stop at `data_size - 1` because
    // we often read pairs of bytes (current and next) when checking markers.
    for (uint32_t i = 0; i < data_size - 1; i++) {
        // Detect JPH start marker (FFD8)
        if (!writing && buffer[i] == JPG_START[0] && buffer[i + 1] == JPG_START[1]) {
            file_count++;
            char name[64];
            sprintf(name, "recovered_file_%d.jpg", file_count);

            out = fopen(name, "wb");
            if (!out) {
                fprintf(stderr, "Error: could not create output file %s\n", name);
                free(buffer);
                return 7;
            }
            writing = 1; // Now writing bytes to the newly created JPG file
        }
        // Write current byte if inside a JPG
        if (writing) {
            fwrite(&buffer[i], 1, 1, out);
        }

        // Check for the JPG end marker while writing. When found, write the
        // final byte, close the file, and stop writing until a new start
        // marker is discovered. We increment `i` to skip the second byte of
        // the end marker because it has already been consumed.
        if (writing && buffer[i] == JPG_END[0] && buffer[i + 1] == JPG_END[1]) {
            fwrite(&buffer[i + 1], 1, 1, out); // Write the last byte of the JPG
            fclose(out);
            out = NULL;
            writing = 0; // Finished writing this JPG

            i++; // Advance past the second byte of the end marker
        }
    }
    free(buffer);

    printf("Recovered %d file(s).\n", file_count);
    return 0;
}