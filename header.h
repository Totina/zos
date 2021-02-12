
#ifndef ZOS_HEADER_H
#define ZOS_HEADER_H

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

#define FS_FILENAME_LENGTH 12
#define DISK_SIZE 2000000       // 2MB
#define CLUSTER_SIZE 1000
#define INODES_COUNT 100
#define MAX_COMMAND_LENGTH 40
#define MAX_FILENAME_LENGTH 12
#define COUNT_DIRECT_LINK 5

#define SPLIT_ARGS_CHAR " "
#define SHELL_CHAR "$"
#define ROOT_CHAR "~"

#define COPY_FILE "cp"
#define MOVE_FILE "mv"
#define REMOVE_FILE "rm"
#define MAKE_DIRECTORY "mkdir"
#define REMOVE_EMPTY_DIRECTORY "rmdir"
#define PRINT_DIRECTORY "ls"
#define PRINT_FILE "cat"
#define CHANGE_DIRECTORY "cd"
#define PRINT_WORKING_DIRECTORY "pwd"
#define INFO "info"
#define FILE_IN "incp"
#define FILE_OUT "outcp"
#define LOAD_COMMANDS "load"
#define FORMAT "format"
#define S_LINK "slink"

#define QUIT "quit"
#define PRINT_FS "printfs"
#define HELP "help"

#define PATH_MAX 4096


typedef struct superblock {
    char signature[10];             // login autora FS
    char volume_descriptor[251];    // popis FS
    int32_t disk_size;              // celkova velikost FS
    int32_t cluster_size;           // velikost clusteru
    int32_t cluster_count;          // pocet clusteru
    int32_t inode_count;            // pocet inodu

    int32_t bitmap_start_address;   // adresa pocatku bitmapy datových bloků
    int32_t inode_start_address;    // adresa pocatku  i-uzlů
    int32_t data_start_address;     // adresa pocatku datovych bloku

} SUPERBLOCK;


typedef struct bitmap {
    int32_t size;
    bool *cluster_free;                 // 1 = true, 0 = false
} BITMAP;

typedef struct directory_item {
    int32_t node_id;
    char item_name[MAX_FILENAME_LENGTH];
} DIRECTORY_ITEM;

typedef struct directory_items {
    int32_t size;
    DIRECTORY_ITEM *data;               // jednotlivé složky
} DIRECTORY_ITEMS;


typedef struct pseudo_inode {
    char id_name[10];
    int32_t node_id;
    int32_t parent_id;

    bool is_free;
    int8_t isDirectory;                 // true = 1, false = 0
    int8_t isSLink;                     // true = 1, false = 0

    int32_t linked_node_id;             // if inode is symbolic link

    int32_t count_clusters;
    int64_t file_size;
    int32_t directs[COUNT_DIRECT_LINK];

    int32_t indirect1;
    int32_t indirect2;
} PSEUDO_INODE;


typedef struct inodes {
    int32_t size;
    PSEUDO_INODE data[INODES_COUNT];
} INODES;

typedef struct file_system {
    SUPERBLOCK *superblock;
    BITMAP *bitmap;
    INODES *inodes;

    PSEUDO_INODE *current_inode;
    DIRECTORY_ITEMS *current_directory;

    char *actual_path;
    char *filename;
    FILE *FILE;
} FS;


// inodes
void assign_clusters(FS *fs, PSEUDO_INODE *inode);

// fs
bool is_absolute_path(char *path);

// main
int commands(FS *fs, char *token);

#endif