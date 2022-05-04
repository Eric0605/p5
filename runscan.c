#include <stdio.h>

#include <stdlib.h>

#include <string.h>

#include "ext2_fs.h"

#include "read_ext2.h"

#include <sys/types.h>

#include <dirent.h>

#include <unistd.h>

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

  uint savedInode[super.s_inodes_per_group];
  int saved = 0;
  long savedFileSize[super.s_inodes_per_group];

  for (unsigned int i = 0; i < super.s_inodes_per_group; i++) {
    // printf("inode %u: \n", i);
    struct ext2_inode * inode = malloc(sizeof(struct ext2_inode));
    read_inode(fd, 0, start_inode_table, i, inode);
    int currentInodeNum = i;
    if (S_ISREG(inode -> i_mode)) {
      uint sizeWanted = inode -> i_size;
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
        savedInode[saved] = currentInodeNum;
        saved++;
        char file[100];
        sprintf(file, "%s/file-%d.jpg", argv[2], currentInodeNum);
        int toWrite = open(file, O_RDWR | O_CREAT | O_TRUNC, 0644);
        for (int i = 0; i < EXT2_N_BLOCKS; i++) {
          if (sizeWanted == 0) {
            break;
          }
          if (inode -> i_block[i] == 0) {
            continue;
          }
          if (i < EXT2_NDIR_BLOCKS) {
            lseek(fd, BLOCK_OFFSET(inode -> i_block[i]), SEEK_SET);
            if (block_size < sizeWanted) {
              read(fd, actualData, block_size);
              write(toWrite, actualData, block_size);
              savedFileSize[saved - 1] += block_size;
              sizeWanted -= block_size;
            } else {
              read(fd, actualData, sizeWanted);
              write(toWrite, actualData, sizeWanted);
              savedFileSize[saved - 1] += sizeWanted;
              sizeWanted -= sizeWanted;
            }
          } else if (i == EXT2_IND_BLOCK) {
            uint indirect1[block_size / 4];
            lseek(fd, BLOCK_OFFSET(inode -> i_block[i]), SEEK_SET);
            read(fd, indirect1, block_size);
            for (unsigned int p = 0; p < block_size / 4; p++) {
              lseek(fd, BLOCK_OFFSET(indirect1[p]), SEEK_SET);
              if (indirect1[p] == 0) {
                break;
              }
              if (block_size < sizeWanted) {
                read(fd, actualData, block_size);
                write(toWrite, actualData, block_size);
                savedFileSize[saved - 1] += block_size;
                sizeWanted -= block_size;
              } else {
                read(fd, actualData, sizeWanted);
                write(toWrite, actualData, sizeWanted);
                savedFileSize[saved - 1] += sizeWanted;
                sizeWanted -= sizeWanted;
              }
            }
          } else if (i == EXT2_DIND_BLOCK) {
            uint indirect1[block_size / 4];
            lseek(fd, BLOCK_OFFSET(inode -> i_block[i]), SEEK_SET);
            read(fd, indirect1, block_size);
            for (unsigned int p = 0; p < block_size / 4; p++) {
              lseek(fd, BLOCK_OFFSET(indirect1[p]), SEEK_SET);
              if (indirect1[p] == 0) {
                break;
              }
              uint indirect2[block_size / 4];
              read(fd, indirect2, block_size);
              for (unsigned int p2 = 0; p2 < block_size / 4; p2++) {
                lseek(fd, BLOCK_OFFSET(indirect2[p2]), SEEK_SET);
                if (indirect2[p2] == 0) {
                  break;
                }
                if (block_size < sizeWanted) {
                  read(fd, actualData, block_size);
                  write(toWrite, actualData, block_size);
                  savedFileSize[saved - 1] += block_size;
                  sizeWanted -= block_size;
                } else {
                  read(fd, actualData, sizeWanted);
                  write(toWrite, actualData, sizeWanted);
                  savedFileSize[saved - 1] += sizeWanted;
                  sizeWanted -= sizeWanted;
                }
              }
            }
          } else if (i == EXT2_TIND_BLOCK) {
            uint indirect1[block_size / 4];
            lseek(fd, BLOCK_OFFSET(inode -> i_block[i]), SEEK_SET);
            read(fd, indirect1, block_size);
            for (unsigned int p = 0; p < block_size / 4; p++) {
              lseek(fd, BLOCK_OFFSET(indirect1[p]), SEEK_SET);
              if (indirect1[p] == 0) {
                break;
              }
              uint indirect2[block_size / 4];
              read(fd, indirect2, block_size);
              for (unsigned int p2 = 0; p2 < block_size / 4; p2++) {
                lseek(fd, BLOCK_OFFSET(indirect2[p2]), SEEK_SET);
                if (indirect2[p2] == 0) {
                  break;
                }
                uint indirect3[block_size / 4];
                read(fd, indirect3, block_size);
                for (unsigned int p3 = 0; p3 < block_size / 4; p2++) {
                  lseek(fd, BLOCK_OFFSET(indirect3[p3]), SEEK_SET);
                  if (indirect3[p3] == 0) {
                    break;
                  }
                  if (block_size < sizeWanted) {
                    read(fd, actualData, block_size);
                    write(toWrite, actualData, block_size);
                    savedFileSize[saved - 1] += block_size;
                    sizeWanted -= block_size;
                  } else {
                    read(fd, actualData, sizeWanted);
                    write(toWrite, actualData, sizeWanted);
                    savedFileSize[saved - 1] += sizeWanted;
                    sizeWanted -= sizeWanted;
                  }
                }
              }
            }
          } else {
            printf("Unknown error from i block number");
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
      for (int j = 0; j < EXT2_N_BLOCKS; j++) {
        if (count == saved) {
          break;
        }
        lseek(fd, BLOCK_OFFSET(inode -> i_block[j]), SEEK_SET);
        read(fd, block, block_size); /* read block from disk*/
        size = 0; /* keep track of the bytes read */
        entry = (struct ext2_dir_entry_2 * ) block; /* first entry in the directory */
        while (size < inode -> i_size) {
          char file_name[EXT2_NAME_LEN + 1];
          memcpy(file_name, entry -> name, entry -> name_len);
          file_name[entry -> name_len] = 0; /* append null char to the file name */
          for (int n = 0; n < saved; n++) {
            if (entry -> inode == savedInode[n]) {
              char target[100];
              sprintf(target, "%s/file-%d.jpg", argv[2], savedInode[n]);
              char toCopyed[entry -> name_len + sizeof(argv[2])];
              sprintf(toCopyed, "%s/%s", argv[2], file_name);
              int fpNum = open(target, O_RDONLY);
              int fpName = open(toCopyed, O_RDWR | O_CREAT | O_TRUNC, 0644);
              char buffer[savedFileSize[n]];
              read(fpNum, buffer, savedFileSize[n]);
              write(fpName, buffer, savedFileSize[n]);
              count += 1;
              close(fpName);
              close(fpName);
            }
          }

          unsigned int nameSize = entry -> name_len * sizeof(char);
          unsigned int nameSize4Based = nameSize;
          nameSize4Based = nameSize4Based / 4;
          nameSize4Based = nameSize4Based * 4;
          if (nameSize4Based < nameSize) {
            nameSize4Based += 4;
          }

          unsigned int toIncrease = sizeof(entry -> inode) + sizeof(entry -> rec_len) + sizeof(entry -> name_len) + sizeof(entry -> file_type) + nameSize4Based;

          entry = (void * ) entry + toIncrease;
          size += toIncrease;
        }
      }

    }
    free(inode);
  }
  close(fd);
}
