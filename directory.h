//
// Created by terez on 2/9/2021.
//

#ifndef ZOS_DIRECTORY_H
#define ZOS_DIRECTORY_H

#include "header.h"

DIRECTORY_ITEMS *create_directory_item(FS *fs, int32_t parent_node_id, PSEUDO_INODE *inode, char *name_directory);
bool find_free_clusters(FS *fs, int32_t count);
bool find_free_node(FS *fs);
void write_directory_items_to_file(FS *fs, DIRECTORY_ITEMS *items, PSEUDO_INODE *inode);
DIRECTORY_ITEMS *read_directory_items_from_file(FS *fs, PSEUDO_INODE *inode);
void print_directory_items(DIRECTORY_ITEMS *items);
static void print_directory_item(DIRECTORY_ITEM *item);

bool directory_contains_file(DIRECTORY_ITEMS *items, char *filename);
void add_item_to_directory(FS *fs, DIRECTORY_ITEMS *items, PSEUDO_INODE *inode, char *name,
                           PSEUDO_INODE *new_inode);

bool create_file_in_FS(FS *fs, FILE *source_file, char *filename, PSEUDO_INODE *dest_inode);
void free_directory_items(DIRECTORY_ITEMS *items);
int32_t *get_all_file_clusters(FS *fs, PSEUDO_INODE *inode);

bool create_s_link(FS *fs, char *filename, char *linked_file_name, PSEUDO_INODE *dest_inode);

#endif //ZOS_DIRECTORY_H
