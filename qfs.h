/*
**
** Header file containing definitions for the QFS (Quinnipiac File System)
**
** Usage: #include "qfs.h"
**
*/

#include <stdio.h>
#include <stdint.h>

#pragma pack(push,1)

// QFS Superblock Structure
typedef struct superblock {
  uint8_t   fs_type;               // File system type/Magic number (0x51 for QFS)
  uint16_t  total_blocks;          // Total number of blocks
  uint16_t  available_blocks;      // Number of blocks available
  uint16_t  bytes_per_block;       // Number of bytes per block
  uint8_t   total_direntries;      // Total number of directory entries
  uint8_t   available_direntries;  // Number of available dir entries
  uint8_t   reserved[8];           // Reserved, all set to 0
  char      label[15];             // NULL-terminated volume label (optional)
} superblock_t;

// QFS Directory Entry Structure
typedef struct direntry {
    char     filename[23];         // NULL-terminated
    uint8_t  permissions;          // File permissions (e.g., read, write, execute)
    uint8_t  owner_id;             // Owner ID
    uint8_t  group_id;             // Group ID
    uint16_t starting_block;       // Starting block number
    uint32_t file_size;            // Size of the file in bytes
} direntry_t;

// QFS File Block Structure (note that the data area size is dynamic based on bytes_per_block)
typedef struct fileblock {
    uint8_t  is_busy;              // Free/busy byte (1 = free, 0 = busy)
    uint8_t  *data;                // Data area (of size bytes_per_block - 3)
    uint16_t next_block;           // Next block number (if applicable)
} fileblock_t;

#pragma pack(pop)