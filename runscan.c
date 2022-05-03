#include <stdio.h>

#include <stdlib.h>

#include <string.h>

#include "ext2_fs.h"

#include "read_ext2.h"

#include <sys/types.h>

#include <dirent.h>

int main(int argc, char ** argv) {
  if (argc != 3) {
    printf("expected usage: ./runscan inputfile outputfile\n");
    exit(0);
  }

  int fd;

  fd = open(argv[1], O_RDONLY); /* open disk image */

  ext2_read_init(fd);

  struct ext2_super_block super;
  struct ext2_group_desc group;
  char removeDir[100];
  sprintf(removeDir, "rm -rf %s", argv[2]);
  system(removeDir);
  int outputDirMade = mkdir(argv[2], S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
  if (outputDirMade == -1) {
    printf("Failing creating output directory %s 1\n", argv[2]);
    exit(-1);
  }
  DIR * outputDir = opendir(argv[2]);
  if (outputDir == 0) {
    printf("Failing creating output directory %s 2\n", argv[2]);
    exit(-1);
  }
  outputDir = opendir(argv[2]);

  // example read first the super-block and group-descriptor
  read_super_block(fd, 0, & super);
  read_group_desc(fd, 0, & group);

  printf("There are %u inodes in an inode table block and %u blocks in the idnode table\n", inodes_per_block, itable_blocks);
  //iterate the first inode block
  off_t start_inode_table = locate_inode_table(0, & group);
  // __u32 savedIndoe[super.s_inodes_per_group];
  int saved = 0;
  for (unsigned int i = 0; i < super.s_inodes_per_group; i++) {
    // printf("inode %u: \n", i);
    struct ext2_inode * inode = malloc(sizeof(struct ext2_inode));
    read_inode(fd, 0, start_inode_table, i, inode);
    int currentInodeNum = i + 1;
    if (S_ISREG(inode -> i_mode)) {
      char buffer[block_size];
      lseek(fd, BLOCK_OFFSET(inode -> i_block[0]), SEEK_SET);
      read(fd, buffer, block_size);
      int is_jpg = 0;
      if (buffer[0] == (char) 0xff && buffer[1] == (char) 0xd8 && buffer[2] == (char) 0xff &&
        (buffer[3] == (char) 0xe0 || buffer[3] == (char) 0xe1 || buffer[3] == (char) 0xe8)) {
        is_jpg = 1;
      }
      char actualData[block_size];
      if (is_jpg) {
        // savedIndoe[saved] = currentInodeNum;
        saved++;
        char file[100];
        sprintf(file, "%s/file-%d.jpg", argv[2], currentInodeNum);
        int toWrite = open(file, O_RDWR | O_CREAT | O_TRUNC, 0644);
        for (int i = 0; i < EXT2_N_BLOCKS; i++) {
          if (inode -> i_block[i] == 0) {
            continue;
          }
          if (i < EXT2_NDIR_BLOCKS) {
            lseek(fd, BLOCK_OFFSET(inode -> i_block[i]), SEEK_SET);
            read(fd, actualData, block_size);
            write(toWrite, actualData, block_size);
          }
          else if (i == EXT2_IND_BLOCK) {
            lseek(fd, BLOCK_OFFSET(inode -> i_block[i]), SEEK_SET);
            char indirect1[4];
            for(unsigned int p = 0; p < block_size / 4; p++){
              lseek(fd, BLOCK_OFFSET(inode -> i_block[i] + 4 * p), SEEK_SET);
              read(fd, indirect1, 4);
              if(indirect1 == 0){
                break;
              }
              else{
                lseek(fd, BLOCK_OFFSET(*indirect1), SEEK_SET);
                read(fd, actualData, block_size);
                write(toWrite, actualData, block_size);
              }
            }
          }
          // else if (i == EXT2_DIND_BLOCK) {
          //   lseek(fd, BLOCK_OFFSET(inode -> i_block[i]), SEEK_SET);
          //   char indirect1[4];
          //   for(unsigned int p = 0; p < block_size / 4; p++){
          //     lseek(fd, BLOCK_OFFSET(inode -> i_block[i] + 4 * p), SEEK_SET);
          //     read(fd, indirect1, 4);
          //     if(indirect1 == 0){
          //       break;
          //     }
          //     else{
          //       char indirect2[4];
          //       for(unsigned int p2 = 0; p2 < block_size / 4; p++){
          //         lseek(fd, BLOCK_OFFSET(*indirect1 + 4 * p2), SEEK_SET);
          //         read(fd, indirect2, 4);
          //         if(indirect2 == 0){
          //           break;
          //         }
          //         else{
          //           lseek(fd, BLOCK_OFFSET(*indirect2), SEEK_SET);
          //           read(fd, actualData, block_size);
          //           write(toWrite, actualData, block_size);
          //         }
          //       }
          //     }
          //   }
          // }  
          else{
            
          }
        }
        close(toWrite);
      }
    }
    free(inode);
  }
  int count = 0;
  for (unsigned int i = 0; i < super.s_inodes_per_group; i++) {
    if (count == saved) {
      break;
    }
    struct ext2_inode * inode = malloc(sizeof(struct ext2_inode));
    read_inode(fd, 0, start_inode_table, i, inode);
    if (S_ISDIR(inode -> i_mode)) {
        struct ext2_dir_entry_2 * entry;
        unsigned int size;
        unsigned char block[block_size];
        lseek(fd, BLOCK_OFFSET(inode -> i_block[0]), SEEK_SET);
        read(fd, block, block_size); /* read block from disk*/
        size = 0; /* keep track of the bytes read */
        entry = (struct ext2_dir_entry_2 * ) block; /* first entry in the directory */
        while (size < inode -> i_size) {
          printf("The current size is %u\n", size);
          char file_name[EXT2_NAME_LEN + 1];
          memcpy(file_name, entry -> name, entry -> name_len);
          file_name[entry -> name_len] = 0; /* append null char to the file name */
          printf("%10u %s\n", entry -> inode, file_name);
          entry = (void * ) entry + entry -> rec_len; /* move to the next entry */
          size += entry -> rec_len;
        }
    }
  }
  close(fd);
}