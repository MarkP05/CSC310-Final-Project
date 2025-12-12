/*Lehan Dharmatilake
 * CSC 310 - Operating Systems Final Project
 * 12/11/2025
 * read_file.c
 *
 * A utility to extract a file stored in a QFS filesystem image.
 *
 * Usage: read_file <filesystem_image> <filename_in_qfs> <output_file>
 *
 * This program opens a QFS filesystem image, locates the specified file in the
 * directory table, follows its linked data blocks, and writes the recovered
 * contents to a local output file. It supports the QFS block structure where
 * each block contains 510 bytes of data followed by a 2-byte pointer to the
 * next block (0xFFFF indicates the end of file).
 */


#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "qfs.h"

int main(int argc, char *argv[]) {

    // ---------------------------------------------------
    // Validate arguments
    // ---------------------------------------------------
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <disk image> <filename> <output file>\n", argv[0]);
        return 1;
    }

    const char *diskimg = argv[1];
    const char *target  = argv[2];
    const char *outfile = argv[3];

    // ---------------------------------------------------
    // Open disk image
    // ---------------------------------------------------
    FILE *fp = fopen(diskimg, "rb");
    if (!fp) {
        perror("fopen(disk image)");
        return 2;
    }

    // ---------------------------------------------------
    // Read superblock
    // ---------------------------------------------------
    superblock_t sb;
    if (fread(&sb, sizeof(superblock_t), 1, fp) != 1) {
        fprintf(stderr, "Error: cannot read superblock\n");
        fclose(fp);
        return 3;
    }

    // ---------------------------------------------------
    // Scan directory entries
    // ---------------------------------------------------
    direntry_t dir;
    int found = 0;

    fseek(fp, sizeof(superblock_t), SEEK_SET);

    for (int i = 0; i < sb.total_direntries; i++) {

        if (fread(&dir, sizeof(direntry_t), 1, fp) != 1) {
            fprintf(stderr, "Error reading directory entry %d\n", i);
            fclose(fp);
            return 4;
        }

        // Skip empty directory entries
        if (dir.filename[0] == '\0')
            continue;

        // Check filename match
        if (strcmp(dir.filename, target) == 0) {
            found = 1;
            break;
        }
    }

    if (!found) {
        fprintf(stderr, "File \"%s\" not found in disk image.\n", target);
        fclose(fp);
        return 5;
    }

    // ---------------------------------------------------
    // Open output file
    // ---------------------------------------------------
    FILE *out = fopen(outfile, "wb");
    if (!out) {
        perror("fopen(output file)");
        fclose(fp);
        return 6;
    }

    // ---------------------------------------------------
    // Begin block traversal
    // ---------------------------------------------------
    uint16_t block = dir.starting_block;
    uint32_t remaining = dir.file_size;

    // Allocate buffer based on block size
    uint8_t *buffer = malloc(sb.bytes_per_block);
    if (!buffer) {
        fprintf(stderr, "Memory allocation failed\n");
        fclose(fp);
        fclose(out);
        return 7;
    }

    // Amount of actual file data per block
    uint32_t data_per_block = sb.bytes_per_block - 3;

    while (remaining > 0 && block != 0xFFFF) {

        // Compute block offset in disk image
        long block_offset =
            sizeof(superblock_t) +
            sizeof(direntry_t) * sb.total_direntries +
            (long)block * sb.bytes_per_block;

        fseek(fp, block_offset, SEEK_SET);

        // Read entire block
        if (fread(buffer, sb.bytes_per_block, 1, fp) != 1) {
            fprintf(stderr, "Error reading block %u\n", block);
            free(buffer);
            fclose(fp);
            fclose(out);
            return 8;
        }

        // Read next block pointer (last two bytes)
        uint16_t next =
            buffer[sb.bytes_per_block - 2] |
            (buffer[sb.bytes_per_block - 1] << 8);

        // Write only file data (skip busy byte)
        uint32_t chunk =
            (remaining > data_per_block) ? data_per_block : remaining;

        fwrite(buffer + 1, chunk, 1, out);

        remaining -= chunk;
        block = next;
    }

    // ---------------------------------------------------
    // Cleanup
    // ---------------------------------------------------
    free(buffer);
    fclose(fp);
    fclose(out);

    printf("Extracted \"%s\" to \"%s\" successfully.\n", target, outfile);
    return 0;
}