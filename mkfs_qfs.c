/*
**Program to make a filesystem on a blank file using the qfs parameters
**
** Usage: mkfs_qfs <disk image file> [<label>]
**
** To create a blank file of a specific size, you can use the following command:
**   dd if=/dev/zero of=<disk image file> bs=1M count=<size in MB>
**
** Example:
**   dd if=/dev/zero of=disk.img bs=1M count=4
**
** Then run:
**   mkfs_qfs disk.img MyVolume
**
** This will format 'disk.img' as a 4MB QFS filesystem with the label 'MyVolume'.
**
*/

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "qfs.h"

int main(int argc, char *argv[]) {

    if (argc < 2 || argc > 3) {
        fprintf(stderr, "Usage: %s <disk image file> [<label>]\n", argv[0]);
        return 1;
    }

    /*
    ** Open the disk image file for reading and writing in binary mode
    **  "rb+" - Open for reading and writing. The file must exist.
    **           This mode is used to modify an existing file.
    **  "wb+" - Open for reading and writing. Create an empty file if
    **           it does not exist or truncate the file to zero length if it does.
    **  "ab+" - Open for reading and writing. Create an empty file if
    **           it does not exist. Writing is always done at the end of the file.
    **           You can use fseek to move the file position indicator for reading.
    **           Note that writes will always append to the end of the file.
    */
    FILE *fp = fopen(argv[1], "rb+");
    if (!fp) {
        perror("fopen");
        return 2;
    }

#ifdef DEBUG
    fprintf(stderr,"Opened disk image: %s\n", argv[1]);
#endif

    // Initialize superblock structure
    superblock_t sb;
    // Set all fields to zero initially
    memset(&sb, 0, sizeof(superblock_t));
    // Set QFS magic number
    sb.fs_type = 0x51; // QFS type

    // Set label if provided
    if (argc == 3) {
        strncpy((char *)sb.label, argv[2], sizeof(sb.label) - 1); // Make sure filename fits
        sb.label[sizeof(sb.label) - 1] = '\0'; // Ensure NULL-termination to be safe

#ifdef DEBUG
    if (argc == 3)
        fprintf(stderr,"Label: %s\n", argv[2]);
#endif

    }

    /*
    ** Determine file size of disk image
    **
    **   fseek sets the file position indicator for the stream pointed to by fp.
    **
    **   The second argument of fseek is the offset in bytes from the location specified by the
    **   third argument. This number can be a positive or negative integer.
    **
    **   The third argument is the reference point for the offset:
    **     SEEK_END sets the position indicator to the end of the file
    **     SEEK_SET sets the position indicator to the beginning of the file
    **     SEEK_CUR sets the position indicator to its current location
    **
    **   ftell returns the current value of the position indicator
    **
    */
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

#ifdef DEBUG
    fprintf(stderr, "File size: %ld bytes\n", file_size);
#endif

    // Calculate block size and counts (TODO: adjust these calculations as needed)
    int total_data_available = (file_size - sizeof(superblock_t) - (sizeof(direntry_t) * 255));
    sb.bytes_per_block = 512;

#ifdef DEBUG
    fprintf(stderr, "Total data available: %d\n", total_data_available);
    fprintf(stderr, "Block size: %d\n", sb.bytes_per_block);
#endif

    sb.total_blocks = total_data_available / sb.bytes_per_block;

#ifdef DEBUG
    fprintf(stderr, "Total blocks: %d\n", sb.total_blocks);
#endif

    sb.available_blocks = sb.total_blocks;

#ifdef DEBUG
    fprintf(stderr, "Available blocks: %d\n", sb.available_blocks);
#endif

    sb.total_direntries = (uint8_t) 255;
    sb.available_direntries = sb.total_direntries;

#ifdef DEBUG
    fprintf(stderr, "Total directory entries: %d\n", sb.total_direntries);
    fprintf(stderr, "Available directory entries: %d\n", sb.available_direntries);
#endif

    // Create empty block (size of zeroed directory entries)
    uint8_t dir_zeros[sizeof(direntry_t) * 255] = {0};

#ifdef DEBUG
    fprintf(stderr, "Size of superblock: %lu bytes\n", sizeof(superblock_t));
    fprintf(stderr, "Size of directory entries area: %lu bytes\n", sizeof(dir_zeros));
    fprintf(stderr, "Data blocks start at byte offset: %lu\n", sizeof(superblock_t) + sizeof(dir_zeros));
#endif

    // Write superblock, directory entries, and data blocks to file
    fwrite(&sb, sizeof(superblock_t), 1, fp);
    fwrite(dir_zeros, sizeof(dir_zeros), 1, fp);

#ifdef DEBUG
    fprintf(stderr,"Clearing data blocks...\n");
#endif

    // Block initialization: mark all data blocks as free (byte 1 of each block = 0)
    uint8_t data = 0x00;
    // Move to first data block
    fseek(fp, sizeof(superblock_t) + sizeof(dir_zeros), SEEK_SET);
    for (int i = 0; i < sb.total_blocks; i++) {
        // Set block busy byte to zero
        fwrite(&data, 1, 1, fp);
        // Move to next block
        fseek(fp, sb.bytes_per_block - 1, SEEK_CUR);
    }

    // Flush and close file [IMPORTANT!]
    fflush(fp);
    fclose(fp);

    return 0;
}