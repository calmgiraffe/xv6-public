#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "fs.h"     // filesystem params
#include "types.h"

#define T_DIR  1   // Directory
#define T_FILE 2   // File
#define T_DEV  3   // Device


struct block {
    char buf[BSIZE];
};

static char *g_fsBasePtr = NULL;
static struct superblock g_sblock;
static uint g_dBlkNumStart;
static int g_initialized = 0;


void inodes_check();
void root_directory_exists();
void dirs_properly_formatted();


// Unallocated blocks have type 0
void inodes_check() {
    assert(g_initialized);

    uint numInodes = g_sblock.ninodes;
    struct dinode *istart = (struct dinode*) (g_fsBasePtr + BSIZE*g_sblock.inodestart);

    for (uint i = 0; i < numInodes; i++) {
        if (istart->type == T_DEV || 
            istart->type == T_DIR || 
            istart->type == T_FILE ||
            istart->type == 0 ) {
                istart++;   
                continue;
        }
        printf("ERROR: bad inode.\n");
        exit(1);
    }   
}


int valid_dblk_num(uint blk) {
    assert(g_initialized);

    if (blk >= g_dBlkNumStart && blk < g_sblock.size) {
        return 1;
    } else {
        return 0;
    }
}


// Root directory exists, its inode number is 1,
// and the parent of the root directory is itself
void root_directory_exists() {
    assert(g_initialized);

    // Root inode = inode num 1
    struct dinode *rooti = (struct dinode*) (g_fsBasePtr + BSIZE*g_sblock.inodestart);
    rooti++;
    
    // Check that inode 1 is type DIR
    if (rooti->type != T_DIR) {
        goto err;
    }

    // Look for directory entry ".."
    const uint direntsPerBlk = BSIZE / sizeof(struct dirent);    
    struct block *const basePtr = (struct block *) (g_fsBasePtr);

    // Iterate through direct ptrs
    for (uint i = 0; i < NDIRECT; i++) {

        // Each direct ptr is actually a blk num
        uint currBlk = rooti->addrs[i];

        if (!valid_dblk_num(currBlk)) {
            // check the next dir ptr
            continue;
        }
        struct block *dBlk = basePtr + currBlk;

        // Iterate through the dirents in the d blk
        for (uint off = 0; off < direntsPerBlk; off++) {
            struct dirent *dirent = ((struct dirent*) dBlk) + off;

            if (!strcmp(dirent->name, "..") && dirent->inum == ROOTINO) {
                // sucess
                return;
            }
        }
    }
    // If no match in direct pointers, iterate through pointer array that is
    // pointed to by indirect ptr

    // NOTE: Assume 1 indirect ptr
    assert(NINDIRECT == 1*BSIZE / sizeof(int));

    uint *indirectBlk = (uint*) (basePtr + rooti->addrs[NDIRECT]);

    for (int i = 0; i < NINDIRECT; i++) {
        uint currBlk = indirectBlk[i];

        if (!valid_dblk_num(currBlk)) {
            continue;
        }
        struct block *dBlk = basePtr + currBlk;

        // Iterate through the dirents in the d blk
        for (uint off = 0; off < direntsPerBlk; off++) {
            struct dirent *dirent = ((struct dirent*) dBlk) + off;

            if (!strcmp(dirent->name, "..") && dirent->inum == ROOTINO) {
                // sucess
                return;
            }
        }
    }

err:
    printf("ERROR: root directory does not exist.\n");
    exit(1);
}


int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <file_system_image>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int fd = open(argv[1], O_RDONLY);
    if (fd == -1) {
        printf("Image not found\n");
        exit(EXIT_FAILURE);
    }

    struct stat st;
    if (fstat(fd, &st) == -1) {
        perror("fstat()");
        close(fd);
        exit(EXIT_FAILURE);
    }
    size_t filesize = st.st_size;

     // Open .img and mmap to address space
    g_fsBasePtr = mmap(NULL, filesize, PROT_READ, MAP_PRIVATE, fd, 0);
    if (g_fsBasePtr == NULL) {
        perror("mmap()");
        close(fd);
        exit(EXIT_FAILURE);
    }

    // Copy superblock's metadata
    // Other globals: fs base ptr, 
    struct superblock *sblk = (struct superblock*) (g_fsBasePtr + BSIZE);
    memcpy(&g_sblock, sblk, sizeof(g_sblock));
    g_dBlkNumStart = g_sblock.size - g_sblock.nblocks;
    g_initialized = 1;

    /*
    // Print all sblock members
    // Superblock correctness check
    printf("size: %u\n", sblk->size);       // 1000
    printf("nblocks: %u\n", sblk->nblocks); // 941
    printf("ninodes: %u\n", sblk->ninodes); // 200
    printf("nlog: %u\n", sblk->nlog);       // 30
    printf("logstart: %u\n", sblk->logstart);     // 2
    printf("inodestart: %u\n", sblk->inodestart); // 32  
    printf("bmapstart: %u\n", sblk->bmapstart);   // 58
    */

    inodes_check();
    root_directory_exists();


    if (munmap(g_fsBasePtr, filesize) == -1) {
        perror("munmap()");
        close(fd);
    }

    printf("All checks passed\n");
    exit(EXIT_SUCCESS);
}