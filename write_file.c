/* Mark Pfister
 * CSC 310 - Operating Systems Final Project (EXTRA CREDIT)
 * 12/12/2025
 * write_file.c
 *
 * Usage:
 *   ./write_file <disk image file> <file to add>
 *
 * Writes a local file into a QFS disk image. The program locates a free
 * directory entry and enough free data blocks, writes the file data across
 * those blocks, links them using the QFS next-block pointer format, and
 * updates the superblock metadata accordingly.
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "qfs.h"

static const char *basename_simple(const char *path) {
    
    // Return the filename portion of a path.
    // Scans forward and updates 'base' after the last '/' or '\\'.
    const char *base = path;
    for (const char *p = path; *p; p++) {
        if (*p == '/' || *p == '\\')
            base = p + 1;
    }
    return base;
}

static long data_region_offset(const superblock_t *sb) {
    // Compute the file-offset where the data region (blocks) begins.
    return (long)sizeof(superblock_t)
         + (long)sizeof(direntry_t) * sb->total_direntries;
}

static long block_offset(const superblock_t *sb, uint16_t block_num) {
    // Offset of a specific data block within the image file.
    return data_region_offset(sb)
         + (long)block_num * sb->bytes_per_block;
}

int main(int argc, char *argv[]) {

    if (argc != 3) {
        fprintf(stderr, "Usage: %s <disk image file> <file to add>\n", argv[0]);
        return 1;
    }

    FILE *fp = fopen(argv[1], "rb+");
    if (!fp) {
        perror("fopen");
        return 2;
    }

#ifdef DEBUG
    printf("Opened disk image: %s\n", argv[1]);
#endif

    // Read and validate superblock
    superblock_t sb;
    if (fread(&sb, sizeof(superblock_t), 1, fp) != 1) {
        fprintf(stderr, "Error reading superblock.\n");
        fclose(fp);
        return 3;
    }

    if (sb.fs_type != 0x51) {
        fprintf(stderr, "Not a valid QFS filesystem.\n");
        fclose(fp);
        return 4;
    }

    // Open local file and compute its size
    FILE *in = fopen(argv[2], "rb");
    if (!in) {
        perror("fopen(local file)");
        fclose(fp);
        return 5;
    }

    fseek(in, 0, SEEK_END);
    long file_size_long = ftell(in);
    fseek(in, 0, SEEK_SET);

    if (file_size_long < 0) {
        fprintf(stderr, "Unable to determine file size.\n");
        fclose(in);
        fclose(fp);
        return 6;
    }

    // Convert size and compute how many blocks are required.
    // Each QFS block reserves 1 byte for a busy marker and
    // 2 bytes for the next-block pointer (little-endian),
    // so usable payload per block = bytes_per_block - 3.
     
    uint32_t file_size = (uint32_t)file_size_long;
    uint32_t data_per_block = sb.bytes_per_block - 3;
    uint32_t blocks_needed =
        (file_size + data_per_block - 1) / data_per_block;
    if (blocks_needed == 0)
        blocks_needed = 1;

    // Quick capacity check: ensure enough free blocks and a free dir entry
    if (sb.available_blocks < blocks_needed ||
        sb.available_direntries == 0) {
        fprintf(stderr, "Not enough space in filesystem.\n");
        fclose(in);
        fclose(fp);
        return 7;
    }

    // Find a free directory entry (first empty filename)
    int dir_index = -1;
    fseek(fp, sizeof(superblock_t), SEEK_SET);

    for (int i = 0; i < sb.total_direntries; i++) {
        direntry_t d;
        fread(&d, sizeof(direntry_t), 1, fp);
        if (d.filename[0] == '\0') {
            dir_index = i;
            break;
        }
    }

    // If no free entry found, bail out
    if (dir_index < 0) {
        fprintf(stderr, "No free directory entry found.\n");
        fclose(in);
        fclose(fp);
        return 8;
    }

    // Find free blocks by scanning each block's busy byte
    // The first byte of a block is treated as a busy flag: 0x00 = free
    uint16_t *blocks = malloc(sizeof(uint16_t) * blocks_needed);
    if (!blocks) {
        fprintf(stderr, "Memory allocation failed.\n");
        fclose(in);
        fclose(fp);
        return 9;
    }

    uint32_t found = 0;
    for (uint16_t b = 0; b < sb.total_blocks && found < blocks_needed; b++) {
        uint8_t busy;
        // Read busy marker (first byte of block)
        fseek(fp, block_offset(&sb, b), SEEK_SET);
        fread(&busy, 1, 1, fp);
        if (busy == 0x00)
            blocks[found++] = b;
    }

    if (found < blocks_needed) {
        fprintf(stderr, "Not enough free blocks.\n");
        free(blocks);
        fclose(in);
        fclose(fp);
        return 10;
    }

    // Save the starting block for the directory entry
    uint16_t starting_block = blocks[0];

    // Write the file across allocated blocks.
    // Block layout used by QFS in this implementation:
    //  [0]    = busy marker (0x01 for in-use)
    //  [1..N] = payload data (up to data_per_block bytes)
    //  [last-2,last-1] = next-block pointer (little-endian uint16)
    //  A next pointer of 0xFFFF denotes end-of-file.
    uint8_t *buffer = malloc(sb.bytes_per_block);
    uint32_t remaining = file_size;

    for (uint32_t i = 0; i < blocks_needed; i++) {
        uint16_t cur = blocks[i];
        uint16_t next = (i + 1 < blocks_needed) ? blocks[i + 1] : 0xFFFF;

        memset(buffer, 0, sb.bytes_per_block);
        // mark block as busy
        buffer[0] = 0x01;

        // copy up to data_per_block bytes from input file
        uint32_t chunk =
            (remaining > data_per_block) ? data_per_block : remaining;
        fread(buffer + 1, 1, chunk, in);

        // write next pointer in little-endian order
        buffer[sb.bytes_per_block - 2] = next & 0xFF;
        buffer[sb.bytes_per_block - 1] = (next >> 8) & 0xFF;

        fseek(fp, block_offset(&sb, cur), SEEK_SET);
        fwrite(buffer, 1, sb.bytes_per_block, fp);

        remaining -= chunk;
    }

    free(buffer);
    free(blocks);
    fclose(in);

    // Populate and write a directory entry for the file
    direntry_t entry;
    memset(&entry, 0, sizeof(entry));

    strncpy(entry.filename, basename_simple(argv[2]),
            sizeof(entry.filename) - 1);
    entry.starting_block = starting_block;
    entry.file_size = file_size;

    fseek(fp,
          sizeof(superblock_t) +
          dir_index * sizeof(direntry_t),
          SEEK_SET);
    fwrite(&entry, sizeof(direntry_t), 1, fp);

    // Update superblock metadata: reduce free counts
    sb.available_blocks -= blocks_needed;
    sb.available_direntries--;

    fseek(fp, 0, SEEK_SET);
    fwrite(&sb, sizeof(superblock_t), 1, fp);

    fclose(fp);

    printf("File \"%s\" written to disk image successfully.\n", entry.filename);
    return 0;
}
