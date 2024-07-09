#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>


#include "fs.h"
#include "param.h"


#define T_DIR  1   // Directory
#define T_FILE 2   // File
#define T_DEV  3   // Device




// TODO: define a macro that accepts inodeNum and returns the bitmap value
// TODO: define macro that accepts a generic block number and returns bitmap value




void inodes_check(struct dinode *base, size_t numInodes) {
    for (size_t i = 0; i < numInodes; i++)
    {
        if (base->type == T_DEV || base->type == T_DIR || base->type == T_FILE)
        {   
            continue;
        } 
        else if (1)
        {
            // check the corresponding inode bit in the bitmap.
        } 

        base++;
    }
            //printf("ERROR: bad inode.\n");

}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <file_system_image>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Open and mmap file to address space
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

    char *fmap = mmap(NULL, filesize, PROT_READ, MAP_PRIVATE, fd, 0);
    if (fmap == NULL) {
        perror("mmap()");
        close(fd);
        exit(EXIT_FAILURE);
    }

    // Record superblock's metadata
    struct superblock *sblk = (struct superblock*) (fmap + ROOTIBSIZE);
    assert(sblk->size == FSSIZE);

    /*
    // Superblock correctness check
    printf("size: %u\n", sblk->size);
    printf("nblocks: %u\n", sblk->nblocks);
    printf("ninodes: %u\n", sblk->ninodes);
    printf("nlog: %u\n", sblk->nlog);
    printf("logstart: %u\n", sblk->logstart);
    printf("inodestart: %u\n", sblk->inodestart);
    printf("bmapstart: %u\n", sblk->bmapstart);
    */

    struct dinode *ibase = (struct dinode *) (fmap + BSIZE*(sblk->inodestart));
    size_t ninodes = sblk->ninodes;
    //char *bmapbase = fmap + BSIZE*(sblk->bmapstart);





    inodes_check(ibase, ninodes);















    if (munmap(fmap, filesize) == -1) {
        perror("munmap()");
        close(fd);
        exit(EXIT_FAILURE);
    }

    printf("All checks passed\n");
    exit(EXIT_SUCCESS);
}