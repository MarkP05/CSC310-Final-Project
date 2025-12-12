#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "qfs.h"

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <disk image file> <file to read> <output file>\n", argv[0]);
        return 1;
    }

    FILE *fp = fopen(argv[1], "rb");
    if (!fp) {
        perror("fopen");
        return 2;
    }

    // Read the superblock
    superblock_t super;
    fseek(fp, 0, SEEK_SET);
    fread(&super, sizeof(superblock_t), 1, fp);

    // Read directory entries
    direntry_t dir;
    int found = 0;
    for (int i = 0; i < super.total_direntries; i++) {
        fseek(fp, sizeof(superblock_t) + i * sizeof(direntry_t), SEEK_SET);
        fread(&dir, sizeof(direntry_t), 1, fp);

        if (dir.filename[0] != '\0' && strcmp(dir.filename, argv[2]) == 0) {
            found = 1;
            break;
        }
    }

    if (!found) {
        fprintf(stderr, "File '%s' not found on disk image.\n", argv[2]);
        fclose(fp);
        return 3;
    }

    FILE *out = fopen(argv[3], "wb");
    if (!out) {
        perror("fopen output file");
        fclose(fp);
        return 4;
    }

    // Read file blocks
    uint16_t block = dir.starting_block;
    uint32_t remaining = dir.file_size;
    uint8_t buffer[super.bytes_per_block];

    while (block != 0xFFFF && remaining > 0) {
        fseek(fp, block * super.bytes_per_block, SEEK_SET);
        fread(buffer, 1, super.bytes_per_block, fp);

        // Write only remaining bytes
        uint32_t to_write = remaining < super.bytes_per_block ? remaining : super.bytes_per_block;
        fwrite(buffer, 1, to_write, out);

        remaining -= to_write;

        // Next block number is at the last 2 bytes of the block (depending on your spec)
        uint16_t *next = (uint16_t*)(buffer + super.bytes_per_block - 2);
        block = *next;
    }

    fclose(out);
    fclose(fp);

    printf("File '%s' has been extracted to '%s'.\n", argv[2], argv[3]);
    return 0;
}
