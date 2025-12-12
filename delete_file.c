/* Mark Pfister
 * CSC 310 - Operating Systems Final Project
 * 12/12/2025
 * delete_file.c
 *
 * Usage:
 *   ./delete_file <disk image file> <file to remove>
 *
 * Removes a file from a QFS disk image by clearing its directory entry
 * and marking all blocks used by the file as free. The superblock is
 * updated to reflect freed blocks and directory entries.
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "qfs.h"

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <disk image file> <file to remove>\n", argv[0]);
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

    // Read superblock from disk
    superblock_t sb;
    if (fread(&sb, sizeof(superblock_t), 1, fp) != 1) {
        fprintf(stderr, "Error reading superblock.\n");
        fclose(fp);
        return 3;
    }

    // Verify filesystem type
    if (sb.fs_type != 0x51) {
        fprintf(stderr, "Not a valid QFS filesystem.\n");
        fclose(fp);
        return 4;
    }

    // Search directory entries for the file to delete
    direntry_t entry;
    int dir_index = -1;

    fseek(fp, sizeof(superblock_t), SEEK_SET);

    for (int i = 0; i < sb.total_direntries; i++) {
        if (fread(&entry, sizeof(direntry_t), 1, fp) != 1) {
            fprintf(stderr, "Error reading directory entry.\n");
            fclose(fp);
            return 5;
        }

        if (entry.filename[0] != '\0' &&
            strcmp(entry.filename, argv[2]) == 0) {
            dir_index = i;
            break;
        }
    }

    // If the file does not exist, exit
    if (dir_index < 0) {
        fprintf(stderr, "File \"%s\" not found.\n", argv[2]);
        fclose(fp);
        return 6;
    }

    // Calculate where the data blocks start
    long data_start =
        sizeof(superblock_t) +
        sizeof(direntry_t) * sb.total_direntries;

    // Traverse the block chain and mark blocks as free
    uint16_t block = entry.starting_block;
    uint32_t freed_blocks = 0;

    while (block != 0xFFFF) {

        long block_offset = data_start +
                            (long)block * sb.bytes_per_block;

        fseek(fp, block_offset, SEEK_SET);

        uint8_t buffer[sb.bytes_per_block];
        fread(buffer, sb.bytes_per_block, 1, fp);

        // Get next block number from last two bytes
        uint16_t next =
            buffer[sb.bytes_per_block - 2] |
            (buffer[sb.bytes_per_block - 1] << 8);

        // Mark block as free
        buffer[0] = 0x00;

        fseek(fp, block_offset, SEEK_SET);
        fwrite(buffer, sb.bytes_per_block, 1, fp);

        freed_blocks++;
        block = next;
    }

    // Clear the directory entry
    memset(&entry, 0, sizeof(entry));

    fseek(fp,
          sizeof(superblock_t) +
          dir_index * sizeof(direntry_t),
          SEEK_SET);
    fwrite(&entry, sizeof(direntry_t), 1, fp);

    // Update superblock counts
    sb.available_blocks += freed_blocks;
    sb.available_direntries++;

    fseek(fp, 0, SEEK_SET);
    fwrite(&sb, sizeof(superblock_t), 1, fp);

    fclose(fp);

    printf("File \"%s\" removed successfully.\n", argv[2]);
    return 0;
}
