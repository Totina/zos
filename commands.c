
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <ctype.h>
#include "header.h"
#include "commands.h"
#include "inodes.h"
#include "fs.h"
#include "directory.h"

/**
 * Přesune soubor z fyzického disku do FS.
 *
 * @param fs - struktura file systému
 * @param token parametry příkazu
 */
void file_in(FS *fs, char *token) {
    char source_filename[MAX_FILENAME_LENGTH];
    PSEUDO_INODE *destination_inode = NULL;

    // get first argument - source_filename file
    token = strtok(NULL, SPLIT_ARGS_CHAR);
    if (token == NULL || strlen(token) < 1) {
        printf("FILE NOT FOUND\n");
        return;
    }
    if (token[strlen(token) - 1] == '\n') {
        token[strlen(token) - 1] = '\0';
    }

    // open the source_filename file
    strcpy(source_filename, token);
    FILE *source_file = fopen(source_filename, "rb");
    if (source_file == NULL) {
        printf("FILE NOT FOUND\n");
        return;
    }

    // get second argument
    token = strtok(NULL, SPLIT_ARGS_CHAR);
    if (token == NULL || token[0] == 0 || strlen(token) == 0) {     // destination was not set
        destination_inode = fs->current_inode;
    } else {
        destination_inode = get_inode(fs, token, 1);          // destination set
    }

    // destination node found
    if (destination_inode == NULL) {
        fclose(source_file);
        return;
    }

    // get filename
    char *filename = get_filename_from_path(source_filename);

    // create file in FS
    bool result = create_file_in_FS(fs, source_file, filename, destination_inode);

    // write changes to FS file
    write_inodes_to_file(fs);
    write_bitmap_to_file(fs);

    // print result
    if (result == true) {
        printf("OK\n");
    } else {
        printf("NOK\n");
    }
}

/**
 * Vytiskne obsah adresáře na obrazovku.
 *
 * @param fs - struktura file systému
 * @param path - cesta do adresáře
 */
void print_directory(FS *fs, char *path) {
    path = strtok(NULL, SPLIT_ARGS_CHAR);

    PSEUDO_INODE *dir = NULL;

    if (path == NULL) {
        dir = fs->current_inode;        // current directory
    } else {
        dir = get_inode(fs, path, 1);
    }

    // directory exists
    if (dir != NULL) {
        DIRECTORY_ITEMS *items = read_directory_items_from_file(fs, dir);

        INODES *inodes = fs->inodes;
        PSEUDO_INODE *current_inode = NULL;

        printf("Total items: %d \n", items->size);
        for (int i = 0; i < items->size; i++) {
            current_inode = &inodes->data[items->data[i].node_id];
            if (current_inode->isDirectory == true) {
                printf("+ SIZE: %ldB, PARENT_ID: %d, NODE_ID: %d, CLUSTERS: %d, NAME: %s \n",
                       current_inode->file_size, current_inode->parent_id, current_inode->node_id,
                       current_inode->count_clusters, items->data[i].item_name);
            } else {
                printf("- SIZE: %ldB, PARENT_ID: %d, NODE_ID: %d, CLUSTERS: %d, NAME: %s \n",
                       current_inode->file_size,
                       current_inode->parent_id, current_inode->node_id, current_inode->count_clusters,
                       items->data[i].item_name);
            }
        }
        free_directory_items(items);
    }
}

/**
 * Vypíše obsah souboru
 *
 * @param fs - struktura file systému
 * @param path - cesta k souboru
 */
void print_file(FS *fs, char *path) {
    // get path
    path = strtok(NULL, SPLIT_ARGS_CHAR);

    // get i-node
    PSEUDO_INODE *inode = get_inode(fs, path, 0);
    PSEUDO_INODE *inode2;

    if (inode == NULL) {
        return;
    }

    // is node symbolic link - if yes, print file it refers to
    if(inode->isSLink == true) {
        //printf("is slink\n");
        inode2 = &fs->inodes->data[inode->linked_node_id];
        //printf("node id: %d\n", inode2->node_id);
        inode = inode2;
    }

    char *buffer = (char *) malloc(fs->superblock->cluster_size * sizeof(int));
    if (buffer == NULL) {
        printf("Error: can't allocate memory for buffer to print file.\n");
        return;
    }

    // get array of all clusters with data
    int32_t *file_clusters = get_all_file_clusters(fs, inode);
    int32_t actual_size = inode->file_size;

    for (int i = 0; i < inode->count_clusters; i++) {
        fseek(fs->FILE, fs->superblock->data_start_address + (file_clusters[i] * fs->superblock->cluster_size), SEEK_SET);

        if (actual_size >= fs->superblock->cluster_size) {
            fread(buffer, sizeof(char), fs->superblock->cluster_size, fs->FILE);
            buffer[fs->superblock->cluster_size] = '\0';    // add end char
            printf("%s", buffer);
            actual_size -= fs->superblock->cluster_size;
        } else {
            fread(buffer, sizeof(char), actual_size, fs->FILE);
            buffer[actual_size] = '\0';     // add end char
            printf("%s", buffer);
            break;
        }
    }
    printf("\n");

    // free memory
    if (file_clusters != NULL) {
        free(file_clusters);
    }
    if (buffer != NULL) {
        free(buffer);
    }
}

/**
 * Vytvoří složky s danou cestou.
 *
 * @param fs - struktura file systému
 * @param path - cesta, kde chceme složku vytvořit
 */
void make_directory(FS *fs, char *path) {
    path = strtok(NULL, SPLIT_ARGS_CHAR);
    //printf("making dir with %s\n", path);

    // path incorrect
    if (path == NULL || path[strlen(path) - 2] == '/') {
        printf("PATH NOT FOUND \n");
        return;
    } else if (path[strlen(path) - 1] == '\n') {
        path[strlen(path) - 1] = '\0';
    }

    char *tmp_path;
    // get name of directory
    char *name =  get_filename_from_path(path);

    // get path to parent node
    tmp_path = get_path_to_parent(path);
    if (strlen(tmp_path) == 0) {
        printf("Error: Directory '%s/%s' could not be created,\n", tmp_path, name);
        return;
    }

    PSEUDO_INODE *parent_inode = NULL;
    if (are_strings_equal(tmp_path, name) == true) {
        parent_inode = fs->current_inode;
    } else {
        parent_inode = get_inode(fs, tmp_path, 1);
    }

    if (parent_inode == NULL) {
        return;
    }

    fs->FILE = fopen(fs->filename, "rb+");
    if (fs->FILE == NULL) {
        perror("Failed: \n");
        return ;
    }

    DIRECTORY_ITEMS *parent_dir = read_directory_items_from_file(fs, parent_inode);

    // directory already contains file with the same name
    if (directory_contains_file(parent_dir, name) == true) {
        printf("EXISTS\n");
        free_directory_items(parent_dir);
        return;
    }

    // get free i-node
    PSEUDO_INODE *new_inode = get_free_inode(fs);
    if (new_inode == NULL) {
        printf("NO FREE I-NODES FOUND\n");
        free_directory_items(parent_dir);
        return;
    }

    // increment number of items in directory
    parent_dir->size = parent_dir->size + 1;

    // realloc memory
    parent_dir->data = realloc(parent_dir->data, parent_dir->size * sizeof(DIRECTORY_ITEM));

    strcpy(parent_dir->data[parent_dir->size - 1].item_name, name);
    parent_dir->data[(parent_dir->size - 1)].node_id = new_inode->node_id;
    parent_inode->file_size = sizeof(DIRECTORY_ITEMS) + parent_dir->size * sizeof(DIRECTORY_ITEM);

    // write to file
    write_directory_items_to_file(fs, parent_dir, parent_inode);
    //lze zamenit za metodu  add_item_to_directory(fs, parent_dir, parent_inode, name, new_inode)

    // new directory
    DIRECTORY_ITEMS *new_dir = create_directory_item(fs, parent_inode->node_id, new_inode, name);
    write_directory_items_to_file(fs, new_dir, new_inode);

    // write to file
    write_bitmap_to_file(fs);
    write_inodes_to_file(fs);

    printf("OK\n");

    // free
    free_directory_items(parent_dir);
}

/**
 * Odstraní soubor nebo prázdný adresář z FS. Neodstraní adresář ve kterém jsou soubory.
 *
 * @param fs - struktura file systému
 * @param path - cesta k položce
 * @param isDirectory - jde o soubor nebo adresář
 */
void remove_file_or_directory(FS *fs, char *path, bool isDirectory) {
    path = strtok(NULL, SPLIT_ARGS_CHAR);

    PSEUDO_INODE *inode_to_remove = NULL;
    char temp_path[strlen(path)];
    strncpy(temp_path, path, 2);
    temp_path[2] = '\0';

    if ((path != NULL && strcmp(path, ".\n") == 0) || (path != NULL && strcmp(temp_path, "..") == 0)) {
        printf("FILE NOT FOUND \n");
        return;
    }

    // get i-node
    if (isDirectory == true) {
        inode_to_remove = get_inode(fs, path, 1);
    } else {
        inode_to_remove = get_inode(fs, path, 0);
    }

    if (inode_to_remove == NULL) {
        return;
    }

    // is directory empty?
    if (isDirectory == true) {
        DIRECTORY_ITEMS *dir = read_directory_items_from_file(fs, inode_to_remove);
        if (dir->size > 2) {
            printf("NOT EMPTY \n");
            return;
        }
        free_directory_items(dir);
    }

    // delete i-node
    bool result = delete_inode(fs, inode_to_remove);
    if (result == false) {
        return;
    }

    // free clusters
    int32_t *file_clusters = get_all_file_clusters(fs, inode_to_remove);

    for (int j = 0; j < inode_to_remove->count_clusters; j++) {
        fs->bitmap->cluster_free[file_clusters[j]] = true;
    }
    if (inode_to_remove->indirect1 != -1) {
        fs->bitmap->cluster_free[inode_to_remove->indirect1] = true;
    }
    if (inode_to_remove->indirect2 != -1) {
        fs->bitmap->cluster_free[inode_to_remove->indirect2] = true;
    }

    int32_t directs[COUNT_DIRECT_LINK];
    for (int j = 0; j < COUNT_DIRECT_LINK; j++) {
        directs[j] = -1;
    }

    // free i-node
    init_pseudoinode(fs, inode_to_remove->node_id, -1, true, false, -1, -1, directs, -1, -1);

    // write to file
    write_bitmap_to_file(fs);
    write_inodes_to_file(fs);

    printf("OK\n");
}

/**
 * Zkopíruje soubor do složky.
 *
 * @param fs - struktura file systému
 * @param token - argumenty příkazu
 */
void copy_file(FS *fs, char *token) {
    // get arguments
    char *src_path = strtok(NULL, SPLIT_ARGS_CHAR);
    char *dest_path = strtok(NULL, SPLIT_ARGS_CHAR);

    PSEUDO_INODE *src_inode = NULL;
    PSEUDO_INODE *dest_inode = NULL;
    PSEUDO_INODE *new_inode = NULL;
    DIRECTORY_ITEMS *dest_dir = NULL;

    // get source i-node
    src_inode = get_inode(fs, src_path, 0);
    char *filename = get_filename_from_path(src_path);
    if (src_inode == NULL) {
        return;
    }

    // get destination i-node
    dest_inode = get_inode(fs, dest_path, 1);
    if (dest_inode == NULL) {
        return;
    }

    // get free i-node
    new_inode = get_free_inode(fs);
    if (new_inode == NULL) {
        printf("NO FREE I-NODES FOUND\n");
        free_directory_items(dest_dir);
        return;
    }

    // get destination directory
    dest_dir = read_directory_items_from_file(fs, dest_inode);
    if (directory_contains_file(dest_dir, filename) == true) {
        printf("FILE ALREADY EXISTS IN THIS DIRECTORY \n");
        free_directory_items(dest_dir);
        return;
    }

    // number of clusters we need
    int32_t n_of_clusters = src_inode->count_clusters;
    if (src_inode->indirect1 != -1) {
        n_of_clusters++;
    }
    if (src_inode->indirect2 != -1) {
        n_of_clusters++;
    }

    if (find_free_clusters(fs, n_of_clusters) == false) {
        printf("NOT ENOUGH FREE CLUSTERS FOUND\n");
    }

    // how many indirect link do we need?
    int32_t cluster_overflow = 0;
    if (src_inode->count_clusters - COUNT_DIRECT_LINK >= 0) {
        cluster_overflow = src_inode->count_clusters - COUNT_DIRECT_LINK;
    }

    int32_t indirects[cluster_overflow];
    int32_t directs[COUNT_DIRECT_LINK];
    for (int i = 0; i < COUNT_DIRECT_LINK; i++) {
        directs[i] = -1;
    }

    int32_t *file_clusters = get_all_file_clusters(fs, src_inode);

    for (int i = 0; i < src_inode->count_clusters; i++) {
        if (i < COUNT_DIRECT_LINK) {
            directs[i] = file_clusters[i];
        } else {
            indirects[i - COUNT_DIRECT_LINK] = file_clusters[i];
        }
    }

    // both indirect links used
    if (src_inode->indirect1 != -1 && src_inode->indirect2 != -1) {
        new_inode = init_pseudoinode(fs, new_inode->node_id, dest_inode->node_id, false, false, src_inode->file_size,
                                     src_inode->count_clusters, directs, get_cluster(fs), get_cluster(fs));
    }
    // only the first indirect link used
    else if (src_inode->indirect1 != -1 && src_inode->indirect2 == -1) {
        new_inode = init_pseudoinode(fs, new_inode->node_id, dest_inode->node_id, false, false, src_inode->file_size,
                                     src_inode->count_clusters, directs, get_cluster(fs), -1);
    }
    // no indirect links
    else {
        new_inode = init_pseudoinode(fs, new_inode->node_id, dest_inode->node_id, false, false, src_inode->file_size,
                                     src_inode->count_clusters, directs, -1, -1);
    }

    // add item to the new directory
    add_item_to_directory(fs, dest_dir, dest_inode, filename, new_inode);

    // data
    char **buffer = malloc(n_of_clusters * sizeof(char *));
    for (int i = 0; i < n_of_clusters; i++) {
        buffer[i] = (char *) malloc((fs->superblock->cluster_size + 1) * sizeof(char));
    }

    int32_t actual_size = new_inode->file_size;
    for (int i = 0; i < new_inode->count_clusters; i++) {
        fseek(fs->FILE, fs->superblock->data_start_address + (file_clusters[i] * fs->superblock->cluster_size),
              SEEK_SET);
        if (actual_size >= fs->superblock->cluster_size) {
            fread(buffer[i], sizeof(char), fs->superblock->cluster_size, fs->FILE);
            buffer[i][fs->superblock->cluster_size] = '\0';
            actual_size -= fs->superblock->cluster_size;
        } else {
            fread(buffer[i], sizeof(char), actual_size, fs->FILE);
            buffer[i][actual_size] = '\0';
        }
    }

    fclose(fs->FILE);
    fs->FILE = fopen(fs->filename, "rb+");


    int32_t n_of_indirects = 0;
    if (new_inode->indirect1 != -1) {
        n_of_indirects++;
    }
    if (new_inode->indirect2 != -1) {
        n_of_indirects++;
    }

    // number of clusters that needs to use indirect links
    int32_t cluster_overflow_2 = new_inode->count_clusters - COUNT_DIRECT_LINK;
    // how many int32 can go to one cluster
    int32_t ints_in_cluster = fs->superblock->cluster_size / sizeof(int32_t);

    // go through clusters
    for (int i = 0; i < new_inode->count_clusters; i++) {
        if (i < COUNT_DIRECT_LINK) {
            fseek(fs->FILE,fs->superblock->data_start_address + (new_inode->directs[i] * fs->superblock->cluster_size), SEEK_SET);
        } else {
            fseek(fs->FILE, fs->superblock->data_start_address + (indirects[i - COUNT_DIRECT_LINK] * fs->superblock->cluster_size), SEEK_SET);
        }

        if (actual_size >= fs->superblock->cluster_size) {
            fwrite(buffer[i], sizeof(char), fs->superblock->cluster_size, fs->FILE);
            actual_size -= fs->superblock->cluster_size;
        } else {
            fwrite(buffer[i], sizeof(char), actual_size, fs->FILE);
            break;
        }
    }

    fflush(fs->FILE);

    // indirect links
    if (n_of_indirects == 1) {
        fseek(fs->FILE, fs->superblock->data_start_address + (new_inode->indirect1 * fs->superblock->cluster_size), SEEK_SET);
        fwrite(indirects, sizeof(int32_t), cluster_overflow_2, fs->FILE);
        fflush(fs->FILE);
    } else if (n_of_indirects == 2) {
        fseek(fs->FILE, fs->superblock->data_start_address + (new_inode->indirect1 * fs->superblock->cluster_size), SEEK_SET);
        fwrite(indirects, sizeof(int32_t), ints_in_cluster, fs->FILE);
        fflush(fs->FILE);

        int32_t data_links[cluster_overflow_2 - ints_in_cluster];
        int index = 0;
        for (int i = ints_in_cluster; i <= ints_in_cluster +(cluster_overflow_2 - ints_in_cluster); ++i) {
            data_links[index] = indirects[i];
            index++;
        }

        fseek(fs->FILE, fs->superblock->data_start_address + (new_inode->indirect2 * fs->superblock->cluster_size), SEEK_SET);

        fwrite(data_links, sizeof(int32_t),(cluster_overflow_2 - ints_in_cluster), fs->FILE);
        fflush(fs->FILE);
    }

    // free memory
    for (int i = 0; i < new_inode->count_clusters; i++) {
        char *currentIntPtr = buffer[i];
        free(currentIntPtr);
    }
    if(buffer) {
        free(buffer);
    }

    // write to file
    write_bitmap_to_file(fs);
    write_inodes_to_file(fs);

    printf("OK\n");
}

/**
 * Přesune soubor do zadané složky.
 *
 * @param fs - struktura file systému
 * @param token - argumenty příkazu - výchozí a cílová cesta
 */
void move_file(FS *fs, char *token) {
    char *src_path = strtok(NULL, SPLIT_ARGS_CHAR);
    char *dest_path = strtok(NULL, SPLIT_ARGS_CHAR);

    // get source i-node
    PSEUDO_INODE *src_inode = get_inode(fs, src_path, 0);
    if (src_inode == NULL) {
        return;
    }

    // get filename
    char *filename = get_filename_from_path(src_path);

    // get destination i-node
    PSEUDO_INODE *dest_inode = NULL;
    dest_inode = get_inode(fs, dest_path, 1);
    if (dest_inode == NULL) {
        // TO DO: RENAME FILE
        printf("New name: %s\n", dest_path);

        PSEUDO_INODE *parent = get_parent_inode(fs, src_inode);
        DIRECTORY_ITEMS *items = read_directory_items_from_file(fs, parent);

        for (int i = 0; i < items->size; ++i) {
            if (items->data[i].node_id == src_inode->node_id) {
                strcpy(items->data[i].item_name, dest_path);
            }
        }
        write_directory_items_to_file(fs, items, parent);
        // write to file
        write_inodes_to_file(fs);
        write_bitmap_to_file(fs);

        return;
    }

    // get directory
    DIRECTORY_ITEMS *directory_destination = read_directory_items_from_file(fs, dest_inode);
    if (directory_contains_file(directory_destination, filename) == true) {
        printf("FILE ALREADY EXISTS THIS IN DIRECTORY \n");
        free_directory_items(directory_destination);
        return;
    }

    // delete file from previous parent directory
    INODES *inodes = fs->inodes;
    PSEUDO_INODE *prev_parent = &inodes->data[src_inode->parent_id];
    bool result = delete_inode(fs, src_inode);
    if (result == false) {
        return;
    }

    // add to the new directory
    src_inode->parent_id = dest_inode->node_id;
    add_item_to_directory(fs, directory_destination, dest_inode, filename, src_inode);

    // write to file
    write_inodes_to_file(fs);
    write_bitmap_to_file(fs);

    free_directory_items(directory_destination);

    printf("OK\n");
}

/**
 * Změní aktuální pracovní adresář, ve kterém se nacházíme.
 *
 * @param fs - struktura file systému
 * @param path - cesta k adresáři
 */
void change_directory(FS *fs, char *path) {
    path = strtok(NULL, SPLIT_ARGS_CHAR);
    char tmp_path[PATH_MAX];

    // get directory on path
    PSEUDO_INODE *dir = NULL;
    if (path == NULL) {
        INODES *inodes = fs->inodes;
        dir = &inodes->data[0];
    } else {
        dir = get_inode(fs, path, 1);
        strncpy(tmp_path, path, 2);
        tmp_path[2] = '\0';
    }

    // directory not found
    if (dir == NULL) {
        return;
    }

    if (dir->node_id == 0) {
        set_path_to_root(fs);
    } else if (dir->node_id != 0 && are_strings_equal(tmp_path, "..") == true) {
        int32_t slash_index = get_last_index_of_slash(fs->actual_path, '/');
        strcpy(tmp_path, fs->actual_path);
        for (int i = 0; i < slash_index; ++i) {
            fs->actual_path[i] = tmp_path[i];
            if (i + 1 == slash_index) {
                fs->actual_path[i + 1] = '\0';
            }
        }
    } else {
        fs->actual_path = get_absolute_path(fs, path);
    }

    // set new working directory
    DIRECTORY_ITEMS *directory = read_directory_items_from_file(fs, dir);
    fs->current_directory = directory;
    fs->current_inode = dir;
}

/**
 * Vytiskne aktuální cestu.
 *
 * @param fs - struktura file systému
 */
void print_working_directory(FS *fs) {
    char path[PATH_MAX];

    if (strlen(fs->actual_path) > 0) {
        strcpy(path, fs->actual_path);
    } else {
        strcpy(path, "/");
    }
    printf("%s \n", path);
}

/**
 * Funkce, která vytiskne informace o souboru
 *
 * @param fs - struktura file systému
 * @param path cesta, která vede k danému souboru
 */
void print_info(FS *fs, char *path) {
    path = strtok(NULL, SPLIT_ARGS_CHAR);

    // get i-node on path
    PSEUDO_INODE *inode = get_inode(fs, path, 2);

    // i-node exists
    if (inode != NULL) {
        PSEUDO_INODE *parent = get_parent_inode(fs, inode);
        DIRECTORY_ITEMS *items = read_directory_items_from_file(fs, parent);

        bool file_found = false;

        for (int i = 0; i < items->size; ++i) {
            if (items->data[i].node_id == inode->node_id) {
                // node is symbolic link
                if(inode->isSLink) {
                    PSEUDO_INODE *inode2 = &fs->inodes->data[inode->linked_node_id];
                    for (int j = 0; j < items->size; ++j) {
                        if (items->data[j].node_id == inode->linked_node_id) {
                            printf("NAME: %s - SIZE: %ldB - I-NODE_ID: %d - ",
                                   items->data[j].item_name, inode2->file_size, inode2->node_id);

                            for (int m = 0; m < COUNT_DIRECT_LINK; m++) {
                                printf("di: %d, ", inode2->directs[m]);
                            }
                            printf("ind: %d, ", inode2->indirect1);
                            printf("ind: %d \n", inode2->indirect2);
                        }
                    }

                }
                // node is a file
                else {
                    printf("NAME: %s - SIZE: %ldB - I-NODE_ID: %d - ", items->data[i].item_name,
                           inode->file_size, inode->node_id);

                    for (int m = 0; m < COUNT_DIRECT_LINK; m++) {
                        printf("di: %d, ", inode->directs[m]);
                    }
                    printf("ind: %d, ", inode->indirect1);
                    printf("ind: %d \n", inode->indirect2);
                }

                file_found = true;
                break;
            }
        }

        if (file_found == false) {
            printf("FILE NOT FOUND\n");
        }
        free_directory_items(items);
    }
}

/**
 * Zkopíruje soubor z FS na pevný disk.
 *
 * @param fs - struktura file systému
 * @param token - argumenty příkazu
 */
void file_out(FS *fs, char *token) {
    char *src_file = strtok(NULL, SPLIT_ARGS_CHAR);
    char *dest_path = strtok(NULL, SPLIT_ARGS_CHAR);

    if (dest_path == NULL) {
        printf("PATH NOT FOUND \n");
        return;
    }
    if (dest_path[strlen(dest_path) - 1] == '\n') {
        dest_path[strlen(dest_path) - 1] = '\0';
    }

    // get source i-node
    PSEUDO_INODE *source_inode = get_inode(fs, src_file, 0);
    char *filename_source = get_filename_from_path(src_file);
    if (source_inode == NULL) {
        return;
    }

    FILE *OUTPUT_FILE = NULL;
    DIR *dir = opendir(dest_path);
    if (ENOENT == errno) {
        printf("PATH NOT FOUND\n");
        closedir(dir);
        return;
    } else if (dir == NULL) {
        printf("PATH NOT FOUND\n");
        closedir(dir);
        return;
    }
    if (dest_path[strlen(dest_path) - 1] == '\n') {
        dest_path[strlen(dest_path) - 1] = '\0';
    }

    char output_file[PATH_MAX];
    strcpy(output_file, dest_path);
    if (dest_path[strlen(dest_path)] != '/') {
        strcat(output_file, "/");
    }
    strcat(output_file, filename_source);

    OUTPUT_FILE = fopen(output_file, "w");
    OUTPUT_FILE = fopen(output_file, "rb+");
    if (fs->FILE == NULL) {
        printf("FILE CANNOT BE CREATED\n");
    } else {
        printf("FILE CREATED\n");
    }

    // write data
    int32_t actual_size = source_inode->file_size;
    char *buffer = (char *) malloc(fs->superblock->cluster_size * sizeof(int));
    if (buffer == NULL) {
        printf("Error: could not allocate memory for buffer\n");
    }

    // get all file clusters
    int32_t *file_clusters = get_all_file_clusters(fs, source_inode);

    // write all file cluster to the new file
    for (int i = 0; i < source_inode->count_clusters; i++) {
        fseek(fs->FILE, fs->superblock->data_start_address + (file_clusters[i] * fs->superblock->cluster_size), SEEK_SET);
        fseek(OUTPUT_FILE, i * (fs->superblock->cluster_size * sizeof(char)), SEEK_SET);

        if (actual_size >= fs->superblock->cluster_size) {
            fread(buffer, sizeof(char), fs->superblock->cluster_size, fs->FILE);
            if (buffer[strlen(buffer)] == '\n') {
                buffer[strlen(buffer)] = '\0';
            } else {
                buffer[strlen(buffer)] = '\0';
            }
            if (buffer[strlen(buffer) - 1] == '\n') {
                buffer[strlen(buffer) - 1] = '\0';
            }

            fwrite(buffer, sizeof(char), fs->superblock->cluster_size, OUTPUT_FILE);
            actual_size -= fs->superblock->cluster_size;
        } else {
            fread(buffer, sizeof(char), actual_size, fs->FILE);
            if (buffer[strlen(buffer)] == '\n') {
                buffer[strlen(buffer)] = '\0';
            } else {
                buffer[strlen(buffer)] = '\0';
            }
            if (buffer[strlen(buffer) - 1] == '\n') {
                buffer[strlen(buffer) - 1] = '\0';
            }
            fwrite(buffer, sizeof(char), actual_size, OUTPUT_FILE);
            break;
        }
    }

    // free
    fflush(OUTPUT_FILE);
    if (file_clusters != NULL) {
        free(file_clusters);
    }
    if (buffer != NULL) {
        free(buffer);
    }
    fclose(OUTPUT_FILE);

    printf("OK\n");
}

/**
 * Načte soubor s příkazy a vykoná je.
 *
 * @param fs - struktura file systému
 * @param token - cesta k  souboru
 */
void load_file_with_commands(FS *fs, char *token) {
    token = strtok(NULL, SPLIT_ARGS_CHAR);
    printf("token in load: %s", token);

    // argument missing
    if (token == NULL || token[0] == 0 || strlen(token) == 0) {
        printf("FILE NOT FOUND \n");
        return;
    }
    if (token[strlen(token) - 1] == '\n') {
        token[strlen(token) - 1] = '\0';
    }

    // open file
    FILE *fp = fopen(token, "r");
    if (fp == NULL) {
        printf("FILE NOT FOUND \n");
        return;;
    }

    //fclose(fs->FILE);

    char buffer[256];
    // -1 to allow room for NULL terminator for really long string
    while (fgets(buffer, 255, fp)) {
        buffer[strcspn(buffer, "\n")] = 0;
        printf("%s\n", buffer);

        //fflush(fs->FILE);
        //fs->FILE = fopen(fs->filename, "rb+");

        char *token2 = strtok(buffer, SPLIT_ARGS_CHAR);
        commands(fs, token2);

        //fclose(fs->FILE);
        //fflush(fs->FILE);
    }

    fclose(fp);

}

/**
 * Naformátuje systém.
 *
 * @param token argumentz příkazu format
 * @param filename název file systému
 * @param signature jméno uživatele
 * @param descriptor informace o systému
 *
 * @return FS struktura
 */
FS *format_fs(char *token, char *filename, char *signature, char *descriptor) {
    token = strtok(NULL, SPLIT_ARGS_CHAR);
    if (token == NULL) {
        printf("CANNOT CREATE FILE\n");
        return NULL;
    }

    int length = index_of_last_digit(token);
    char number[length + 1];
    memset(number, 0, length + 1);
    strncpy(number, token, length);
    if (number[0] == 0) {
        printf("CANNOT CREATE FILE\n");
        return NULL;
    } else if (number[0] == '0') {
        printf("CANNOT CREATE FILE\n");
        return NULL;
    }

    int real_size = (strlen(token) - length) - 1;
    char multiple[real_size];

    int j = 0;
    for (int i = length; i < strlen(token) - 1; i++) {
        multiple[j] = token[i];
        j++;
    }
    int real_number = handle_bytes(multiple, real_size);
    int disk_size = atoi(number) * real_number;
    if (disk_size <= 0) {
        printf("CANNOT CREATE FILE\n");
        return NULL;
    }

    // initialize FS
    FS *fs = fs_init(filename, signature, descriptor, disk_size, 0);

    printf("OK\n");

    return fs;
}

/**
 *
 * Stará se o převod jednotek.
 *
 * @param size zadaný řetězec vleikosti pr oformátování
 * @param size_digits počet čísel
 *
 * @return násobek
 */
int handle_bytes(char *size, int size_digits) {
    int multiple_number = 1;
    char unit[size_digits];
    strncpy(unit, size, size_digits);

    switch(size_digits) {
        case 2:
            switch(unit[0]) {
                case 'G':
                    multiple_number = 1000 * 1000 * 1000;
                    break;
                case 'M':
                    multiple_number = 1000 * 1000;
                    break;
                case 'K':
                    multiple_number = 1000;
                    break;
                case 'B':
                    multiple_number = 1;
                    break;
                default:
                    printf("Wrong unit.\n");
                    break;
            }
            break;
        default:
            printf("Wrong unit.\n");
            break;
    }

    return multiple_number;
}

/**
 * Vrátí delku čísla v řetězci, resp. index poslední číslice v řetězci.
 *
 * @param number - řetězec znaků
 *
 * @return délka řetězce číslic
 */
int index_of_last_digit(char *number) {
    char tmp[strlen(number)];
    strcpy(tmp, number);

    int index = 0;
    while(isdigit(tmp[index])) {
        index++;
    }

    return index;
}

/**
 * Vytvoří symbolický link na soubor.
 *
 * @param fs - struktura file systému
 * @param token - argumenty příkazu
 */
void create_slink(FS *fs, char *token) {
    char source_filename[MAX_FILENAME_LENGTH];  // name of the file we want to link to
    char link_name[MAX_FILENAME_LENGTH];
    PSEUDO_INODE *destination_inode = NULL;

    // get first argument - source_filename file
    token = strtok(NULL, SPLIT_ARGS_CHAR);
    if (token == NULL || strlen(token) < 1) {
        printf("FILE NOT FOUND\n");
        return;
    }
    if (token[strlen(token) - 1] == '\n') {
        token[strlen(token) - 1] = '\0';
    }
    strcpy(source_filename, token);

    //printf("Creating link to file: %s", source_filename);

    // get second argument
    token = strtok(NULL, SPLIT_ARGS_CHAR);
    if (token == NULL || strlen(token) < 1) {
        printf("PATH NOT FOUND\n");
        return;
    }
    if (token[strlen(token) - 1] == '\n') {
        token[strlen(token) - 1] = '\0';
    }
    strcpy(link_name, token);
    //printf("Creating link with name: %s", link_name);

    // destination
    destination_inode = fs->current_inode;

    // create file
    create_s_link(fs, link_name, source_filename, destination_inode);

    // write changes to FS file
    write_inodes_to_file(fs);
    write_bitmap_to_file(fs);

    // print result
    printf("OK\n");
}

/**
 * Vypíše všechny existující příkazy na obrazovku.
 */
void print_help() {
    printf("\n--- Commands ---\n");
    printf("%s - Copy file (%s s1 s2)\n", COPY_FILE, COPY_FILE);
    printf("%s - Move file (%s s1 s2)\n", MOVE_FILE, MOVE_FILE);
    printf("%s - Remove file (%s s1)\n", REMOVE_FILE, REMOVE_FILE);
    printf("%s - Make directory (%s a1)\n", MAKE_DIRECTORY, MAKE_DIRECTORY);
    printf("%s - Remove empty directory (%s a1)\n", REMOVE_EMPTY_DIRECTORY, REMOVE_EMPTY_DIRECTORY);
    printf("%s - Print directory (%s a1)\n", PRINT_DIRECTORY, PRINT_DIRECTORY);
    printf("%s - Print file (%s s1)\n", PRINT_FILE, PRINT_FILE);
    printf("%s - Change directory (%s s1)\n", CHANGE_DIRECTORY, CHANGE_DIRECTORY);
    printf("%s - Print working directory (%s)\n", PRINT_WORKING_DIRECTORY, PRINT_WORKING_DIRECTORY);
    printf("%s - Print info about file or directory (%s s1/a1)\n", INFO, INFO);
    printf("%s - Copy file from HD to FS (%s s1 s2)\n", FILE_IN, FILE_IN);
    printf("%s - Copy file from FS to HD (%s s1 s2)\n", FILE_OUT, FILE_OUT);
    printf("%s - Load commands from file (%s s1)\n", LOAD_COMMANDS, LOAD_COMMANDS);
    printf("%s - Format file system (%s 600MB)\n", FORMAT, FORMAT);
    printf("%s - Create symbolic link (%s s1 s2)\n", S_LINK, S_LINK);

    printf("%s - Print file system (%s)\n", PRINT_FS, PRINT_FS);
    printf("%s - Quit (%s)\n\n", QUIT, QUIT);
}