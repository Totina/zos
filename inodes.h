//
// Created by terez on 2/9/2021.
//

#ifndef ZOS_INODES_H
#define ZOS_INODES_H

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include "header.h"



PSEUDO_INODE *get_free_inode(FS *fs);
INODES *inodes_init(int32_t count);
int32_t  get_cluster(FS *fs);
void write_inodes_to_file(FS *fs);
void print_inodes(INODES *inodes);

PSEUDO_INODE *get_inode(FS *fs, char *path, int32_t type);
PSEUDO_INODE *search_for_inode(FS *fs, PSEUDO_INODE *start_inode, char *path, bool search_from_current_node);

bool are_strings_equal(char *string1, char *string2);
bool contains_char(char *string, char pattern);

char *get_filename_from_path(char *path);
int32_t get_last_index_of_slash(char *string, char pattern);

PSEUDO_INODE *init_pseudoinode(FS *fs, int32_t id_node, int32_t parent_id, bool isFree, bool isDirectory, int32_t file_size,
                 int32_t count_clusters,
                 int32_t directs[COUNT_DIRECT_LINK], int32_t indirect1, int32_t indirect2);

void write_clusters_to_file(FS *fs, PSEUDO_INODE *new_inode, int32_t *clusters, char **buffer);
char *get_path_to_parent(char *path);

bool delete_inode(FS *fs, PSEUDO_INODE *inode);
PSEUDO_INODE *get_parent_inode(FS *fs, PSEUDO_INODE *inode);

void read_inodes_from_file(FS *fs);

PSEUDO_INODE * init_slink(FS *fs, int32_t id_node, int32_t parent_id, int32_t linked_id,
                          int32_t directs[COUNT_DIRECT_LINK]);

#endif //ZOS_INODES_H
