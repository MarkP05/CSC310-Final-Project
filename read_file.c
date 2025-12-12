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

/*
   read_file <diskimage> <filename> <outputfile>
*/

int main(int argc, char *argv[]) {

    if (argc != 4) {
        fprintf(stderr, "Usage: %s <disk image> <filename> <output file>\n", argv[0]);
        return 1;
    }

    const char *diskimg = argv[1];
    const char *target  = argv[2];
    const char *outfile = argv[3];

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

    // Directory entries start immediately after superblock
    direntry_t dir;
    int found = 0;
    long dir_start = sizeof(superblock_t);

    fseek(fp, dir_start, SEEK_SET);

    // ---------------------------------------------------
    // Scan directory entries
    // ---------------------------------------------------
    for (int i = 0; i < sb.total_direntries; i++) {

        if (fread(&dir, sizeof(direntry_t), 1, fp) != 1) {
            fprintf(stderr, "Error reading directory entry %d\n", i);
            fclose(fp);
            return 4;
        }

        if (dir.filename[0] == 0)
            continue;  // empty entry

        if (strcmp((char*)dir.filename, target) == 0) {
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

    while (block != 0xFFFF && remaining > 0) {

        // Compute location of block in disk
        long block_offset = sizeof(superblock_t)
                          + sizeof(direntry_t) * sb.total_direntries
                          + (long)block * sb.bytes_per_block;

        fseek(fp, block_offset, SEEK_SET);

        uint8_t buffer[512];
        if (fread(buffer, sb.bytes_per_block, 1, fp) != 1) {
            fprintf(stderr, "Error reading block %u\n", block);
            fclose(fp);
            fclose(out);
            return 7;
        }

        // Last 2 bytes = next block number
        uint16_t next = buffer[510] | (buffer[511] << 8);

        // Data = first 510 bytes (or fewer if last block)
        uint32_t chunk = (remaining > 510) ? 510 : remaining;
        fwrite(buffer, chunk, 1, out);

        remaining -= chunk;
        block = next;
    }

    fclose(fp);
    fclose(out);

    printf("Extracted \"%s\" to \"%s\" successfully.\n", target, outfile);
    return 0;
}
