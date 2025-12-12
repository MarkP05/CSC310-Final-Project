/*
Michael Alvarado
CSC310 - Operating Systems Final Project
12/11/25

Usage: list_information <filesystem_image>

This program reads in the QFS disk image and list information about the disk image
from the superblock and its directory contents

*/

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "qfs.h"

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <disk image file>\n", argv[0]);
        return 1;
    }
    FILE *fp = fopen(argv[1], "rb");
    if (!fp) {
        perror("fopen");
        return 2;
    }

    superblock_t sblock;
    fseek(fp, 0, SEEK_SET);
    fread(&sblock, sizeof(superblock_t), 1, fp);

    //a check where the QFS value should be 0x51
    //if the file is incorrect then the program stops
    if(sblock.fs_type != 0x51){
        fprintf(stderr, "Not a valid QFS filesystem image.\n");
        fclose(fp);
        return 3;
    }

    //printing out the information
    printf("Block size: %u\n", sblock.bytes_per_block);
    printf("Total number of blocks: %u\n", sblock.total_blocks);
    printf("Number of free blocks: %u\n", sblock.available_blocks);
    printf("Total number of directory entries: %u\n", sblock.total_direntries);
    printf("Number of free directory entries: %u\n", sblock.available_direntries);

    //directory entries start at offset 32 when the total amount of entries is 255
    fseek(fp, 32, SEEK_SET);

    for (int i = 0; i < 255; i++){
        direntry_t directoryEntry;
        fread(&directoryEntry, sizeof(direntry_t), 1, fp);

        //if the entry is in use if file isn't empty
        if(directoryEntry.filename[0] != '\0'){
            char name[24];
            memcpy(name, directoryEntry.filename, 23);
            name[23] = '\0';

            printf("%s\t%u\t%u\n", name, directoryEntry.file_size, directoryEntry.starting_block);
        }
    }




#ifdef DEBUG
    printf("Opened disk image: %s\n", argv[1]);
#endif

    fclose(fp);
    return 0;
}