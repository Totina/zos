//
// Created by terez on 2/9/2021.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "header.h"
#include "fs.h"
#include "inodes.h"
#include "directory.h"

/**
 * Inicializuje file systém. V závislosti na tom, zda soubor file systému již existuje, nebo chceme
 * soubor formátovat, buď:
 *  -> naformátuje soubor a zapíše do něj základní strukturu file systému
 *  -> načte soubor s file systémem
 *
 * @param filename název file sytému
 * @param signature jméno uživatele
 * @param descriptor popis systému
 * @param disk_size velikost disku
 * @param format 0 ano, formátovat, 1 ne,
 *
 * @return struktura file systému
 */
FS *fs_init(char *filename, char *signature, char *descriptor, size_t disk_size, int format) {
    printf("FS initializing. \n");

    // allocation
    FS *fs = calloc(1, sizeof(FS));
    fs->filename = calloc(FS_FILENAME_LENGTH, sizeof(char));

    // filename
    strcpy(fs->filename, filename);

    // allocation
    fs->actual_path = calloc(PATH_MAX, sizeof(char));
    memset(fs->actual_path, 0, PATH_MAX * sizeof(char));

    // try to open the file with FS
    FILE *fs_file = fopen(filename, "r+");

    // FS doesn't exists yet OR we wanna format the FS
    if (fs_file == NULL || format == 0) {
        printf("Formatting the FS\n");

        // create new file
        fs->FILE = fopen(fs->filename, "wb");

        // initialize superblock
        SUPERBLOCK *superblock = superblock_init(signature, descriptor, disk_size, CLUSTER_SIZE);
        fs->superblock = superblock;

        // initialize bitmap
        BITMAP *bitmap = bitmap_init(fs->superblock->cluster_count);
        fs->bitmap = bitmap;

        // initialize inodes
        INODES *inodes = inodes_init(fs->superblock->inode_count);
        fs->inodes = inodes;

        // create FS file with the basic structure
        create_file(fs);

    } else {
        // loading already existing FS
        fclose(fs_file);
        printf("File system file (%s) found, loading.\n", filename);

        fs->FILE = fopen(fs->filename, "rb+");

        if (fs->FILE == NULL) {
            printf("Error: File system file does not exist.\n");
            return NULL;
        }

        // load fs from file
        load_fs_from_file(fs);

        INODES *inodes = fs->inodes;
        fs->current_inode = &inodes->data[0];

        DIRECTORY_ITEMS *root_directory = read_directory_items_from_file(fs, fs->current_inode);
        fs->current_directory = root_directory;
    }

    update_current_directory(fs);
    print_fs(fs);

    return fs;
}

/**
 * Načte file systém ze souboru.
 *
 * @param fs - struktura file systému
 */
void load_fs_from_file(FS *fs) {
    read_sb_from_file(fs);
    read_bitmap_from_file(fs);
    read_inodes_from_file(fs);

    /*INODES *inodes = fs->inodes;
    fs->current_inode = &inodes->data[0];*/
}

/**
 * Inicializuje strukturu superblocku FS.
 *
 * @param signature - jméno uživatele
 * @param volume_descriptor - popis systému
 * @param disk_size - velikost disku
 * @param cluster_size - počet datových bloků
 *
 * @return struktura superblocku
 */
SUPERBLOCK *superblock_init(char *signature, char *volume_descriptor, int32_t disk_size, int32_t cluster_size) {
    // allocation
    SUPERBLOCK *superblock = calloc(1, sizeof(SUPERBLOCK));

    // signature
    strcpy(superblock->signature, signature);

    // volume descriptor
    strcpy(superblock->volume_descriptor, volume_descriptor);

    // set attributes of superblock
    superblock->inode_count = INODES_COUNT;
    superblock->disk_size = disk_size;
    superblock->cluster_size = cluster_size;
    superblock->cluster_count = disk_size / cluster_size;       // cluster count = disk size / cluster size

    if (disk_size % cluster_size != 0) superblock->cluster_count++;

    // bitmap start address after superblock
    superblock->bitmap_start_address = sizeof(SUPERBLOCK);

    // inode start address after bitmap
    superblock->inode_start_address = (superblock->bitmap_start_address + sizeof(BITMAP) + (superblock->cluster_count * sizeof(bool)));

    // data start address after inodes
    superblock->data_start_address = (superblock->inode_start_address + sizeof(INODES));

    return superblock;
}

/**
 * Inicializuje strukturu bitmapy.
 *
 * @param cluster_count počet datových bloků
 *
 * @return struktura bitmapy
 */
BITMAP *bitmap_init(int32_t cluster_count) {
    // allocation
    BITMAP *bitmap = calloc(1, sizeof(BITMAP));

    // bitmap size
    bitmap->size = cluster_count;

    // allocation of array of bools and set to true (free)
    bitmap->cluster_free = calloc(bitmap->size, sizeof(bool));
    memset(bitmap->cluster_free, true, bitmap->size);

    return bitmap;
}

/**
 * Zapíše do souboru file systému základní struktury FS.
 *
 * @param fs - struktura file systému
 */
void create_file(FS *fs) {
    printf("Creating file.\n");

    if (fs->FILE == NULL) {
        printf("Error creating FS file.\n");
        return;
    } else {
        //initialize ROOT
        PSEUDO_INODE *inode_root = get_free_inode(fs);      // find free i-node
        DIRECTORY_ITEMS *directory_root = create_directory_item(fs, inode_root->node_id, inode_root, "ROOT");
        fs->current_inode = inode_root;
        fs->current_directory = directory_root;

        // write FS structures to the file
        write_superblock_to_file(fs);
        write_bitmap_to_file(fs);
        write_inodes_to_file(fs);
        // write ROOT
        write_directory_items_to_file(fs, directory_root, inode_root);
    }
}

/**
 * Zapíše strukturu superblocku do souboru file systému.
 *
 * @param fs - struktura file systému
 */
void write_superblock_to_file(FS *fs) {
    fseek(fs->FILE, 0, SEEK_SET);
    fwrite(fs->superblock, sizeof(SUPERBLOCK), 1, fs->FILE);
    fflush(fs->FILE);
}

/**
 * Zapíše strukturu bitmapy do souboru file systému.
 *
 * @param fs - struktura file systému
 */
void write_bitmap_to_file(FS *fs) {
    fseek(fs->FILE, fs->superblock->bitmap_start_address, SEEK_SET);
    fwrite(fs->bitmap, sizeof(BITMAP), 1, fs->FILE);
    fflush(fs->FILE);

    fseek(fs->FILE, fs->superblock->bitmap_start_address + sizeof(BITMAP), SEEK_SET);
    fwrite(fs->bitmap->cluster_free, sizeof(bool), fs->bitmap->size, fs->FILE);
    fflush(fs->FILE);
}

/**
 * Přečte obsah aktuálního adresáře ze souboru file systému a nastaví ho jako obsah aktuálního adresáře.
 *
 * @param fs - struktura file systému
 */
void update_current_directory(FS *fs) {
    // read items in this directory from file
    DIRECTORY_ITEMS *items = read_directory_items_from_file(fs, fs->current_inode);
    fs->current_directory = items;
}

/**
 * Vypíše informace o file systému na obrazovku.
 *
 * @param fs - struktura file systému
 */
void print_fs(FS *fs) {
    printf("\nFILE SYSTEM\n");
    printf("Current path: %s:%s%s%s \n", fs->superblock->signature, ROOT_CHAR, fs->actual_path, SHELL_CHAR);
    printf("Current i-node id: %d \n", fs->current_inode->node_id);
    printf("Current directory: \n");
    print_directory_items(fs->current_directory);
    print_superblock(fs->superblock);
    print_bitmap(fs->bitmap);
    print_inodes(fs->inodes);
    printf("\n");
}

/**
 * Vypíše informace o superblocku.
 *
 * @param superblock - struktura superblocku k výpisu
 */
void print_superblock(SUPERBLOCK *superblock) {
    printf("--- SUPERBLOCK --- \n");
    printf("Signature: %s\n", superblock->signature);
    printf("Volume descriptor: %s\n", superblock->volume_descriptor);
    printf("Disk size: %dB (%dKB) (%dMB)\n", superblock->disk_size, superblock->disk_size / 1000, superblock->disk_size / 1000000);
    printf("Cluster size: %dB\n", superblock->cluster_size);
    printf("Cluster count: %d\n", superblock->cluster_count);
    printf("INODES count: %d\n", superblock->inode_count);
    printf("Bitmap start address: %d\n", superblock->bitmap_start_address);
    printf("INODE start address: %d\n", superblock->inode_start_address);
    printf("Data start address: %d\n", superblock->data_start_address);
    printf("\n");
}

/**
 * Vypíše informace o bitmapě a bitmapu samotnou.
 *
 * @param bitmap - struktura bitmapy k výpisu
 */
void print_bitmap(BITMAP *bitmap) {
    printf("--- BITMAP ---\n");
    printf("Bitmap size: %d\n", bitmap->size);
    for (int i = 0; i < bitmap->size; ++i) {
        if (bitmap->cluster_free[i] == true) {
            printf("[%d - free], ", i);
        }
        if (bitmap->cluster_free[i] == false) {
            printf("[%d - not free], ", i);
        }
    }
    printf("\n");
}

/**
 * Přečte strukturu superblock ze souboru file systému.
 *
 * @param fs - struktura file systému
 */
void read_sb_from_file(FS *fs) {
    // superblock exists -> free and set to null
    if (fs->superblock != NULL) {
        free(fs->superblock);
        fs->superblock = NULL;
    }

    SUPERBLOCK *sb = calloc(1, sizeof(SUPERBLOCK));
    fseek(fs->FILE, 0, SEEK_SET);
    fread(sb, sizeof(SUPERBLOCK), 1, fs->FILE);

    fs->superblock = sb;
}

/**
 * Přečte strukturu bitmapy ze souboru file systému.
 *
 * @param fs - struktura file systému
 */
void read_bitmap_from_file(FS *fs) {
    // bitmap exists -> free and set to null
    if (fs->bitmap != NULL) {
        free(fs->bitmap);
        fs->bitmap = NULL;
    }

    BITMAP *bitmap = calloc(1, sizeof(BITMAP));

    // read bitmap structure
    fseek(fs->FILE, fs->superblock->bitmap_start_address, SEEK_SET);
    fread(bitmap, sizeof(BITMAP), 1, fs->FILE);

    // read clusters
    bitmap->cluster_free = calloc(bitmap->size, sizeof(bool));
    fseek(fs->FILE, fs->superblock->bitmap_start_address + sizeof(BITMAP), SEEK_SET);
    fread(bitmap->cluster_free, bitmap->size * sizeof(bool), 1, fs->FILE);

    fs->bitmap = bitmap;
}

/**
 * Vrátí, zda je cesta absolutní, podle toho, zda první znak v zadané cestě je lomítko.
 *
 * @param path cesta
 * @return  true, cesta je absolutní
 *          false, cesta je relativní
 */
bool is_absolute_path(char *path) {
    if(path[0] == 47){      // 47 = '/'
        return true;
    } else {
        return false;
    }
}

/**
 * Nastaví cestu na root.
 *
 * @param fs - struktura file systému
 */
void set_path_to_root(FS *fs) {
    memset(fs->actual_path, 0, PATH_MAX);
}

/**
 * Vrátí absolutní cestu k souboru.
 *
 * @param fs - struktura file systému
 * @param path - relativní cesta
 */
char *get_absolute_path(FS *fs, char *path) {
    char *temp_path = calloc(PATH_MAX, sizeof(char));

    if (strlen(path) > 0 && path[strlen(path) - 1] == '\n'){
        path[strlen(path) - 1] = '\0';
    }

    if(is_absolute_path(path) == true){
        // absolute path
        strcpy(temp_path, path);

    } else {
        // relative path - adding path to the current directory
        strcpy(temp_path, fs -> actual_path);

        if (strlen(path) > 0 && path[0] != 47) {
            strcat(temp_path, "/");
        }
        strcat(temp_path, path);
    }

    return temp_path;
}