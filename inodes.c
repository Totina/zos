//
// Created by terez on 2/9/2021.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "inodes.h"
#include "directory.h"
#include "header.h"

/**
 * Inicializuje strukturu inodes.
 *
 * @param count počet inodů
 *
 * @return struktura inodes
 */
INODES *inodes_init(int32_t count) {
    char tmp[10];
    memset(tmp, 0, 8);

    // allocation
    INODES *inodes = calloc(1, sizeof(INODES));

    // set size
    inodes->size = count;

    // set attributes of inodes
    for (int i = 0; i < inodes->size; i++) {
        inodes->data[i].node_id = i;
        inodes->data[i].parent_id = -1;

        inodes->data[i].is_free = true;
        inodes->data[i].isDirectory = false;
        inodes->data[i].isSLink = false;

        inodes->data[i].linked_node_id = -1;

        inodes->data[i].indirect1 = -1;
        inodes->data[i].indirect2 = -1;

        inodes->data[i].count_clusters = -1;
        inodes->data[i].file_size = -1;

        for (int j = 0; j < COUNT_DIRECT_LINK; j++) {
            inodes->data[i].directs[j] = -1;
        }

        snprintf(tmp, 10, "node%d", i);     // /0
        strcpy(inodes->data[i].id_name, tmp);
        memset(tmp, 0, 8);
    }
    return inodes;
}

/**
 * Vrátí volný i-node.
 *
 * @param fs - struktura file systému
 * @return  volný inode
 */
PSEUDO_INODE *get_free_inode(FS *fs) {
    // go through inodes and return the first free i-node
    for (int i = 0; i < fs->inodes->size; ++i) {
        if (fs->inodes->data[i].is_free == true)  {
            return &fs->inodes->data[i];
        }
    }

    // no free i-nodes found
    printf("No free i-node found.\n");
    return NULL;
}

/**
 * Najde volné clustery a přiřadí je danému i-nodu (resp. directory item).
 *
 * @param fs - struktura file systému
 * @param inode inode directory
 */
void assign_clusters(FS *fs, PSEUDO_INODE *inode) {
    // how many clusters we need
    int32_t n_of_clusters = inode->file_size / fs->superblock->cluster_size;

    // nevychazi na cele clustery, musime pridat dalsi
    if (inode->file_size % fs->superblock->cluster_size > 0) {
        n_of_clusters++;
    }

    inode->count_clusters = n_of_clusters;

    for (int i = 0; i < n_of_clusters; ++i) {
        // get index of free cluster
        int index_of_free_cluster = get_cluster(fs);

        inode->directs[i] = index_of_free_cluster;
        fs->bitmap->cluster_free[index_of_free_cluster] = false;
    }
}

/**
 * Vrátí index volného clusteru.
 *
 * @param fs - struktura file systému
 * @return      index volného clusteru
 *              -1, pokud volný cluster neexistuje
 */
int32_t  get_cluster(FS *fs){
    for (int i = 0; i < fs->bitmap->size; ++i) {
        if(fs->bitmap->cluster_free[i] == true)  {
            return i;
        }
    }

    printf("Free cluster not found.\n");
    return -1;
}

/**
 * Zapíše i-nodes strukturu do souboru file sustému.
 *
 * @param fs - struktura file systému
 */
void write_inodes_to_file(FS *fs) {
    fseek(fs->FILE, fs->superblock->inode_start_address, SEEK_SET);
    fwrite(fs->inodes, sizeof(INODES), 1, fs->FILE);
    fflush(fs->FILE);
}

/**
 * Přečte i-nodes strukturu ze souboru file systému.
 *
 * @param fs - struktura file systému
 */
void read_inodes_from_file(FS *fs) {
    // inodes exist -> free and set to null
    if (fs->inodes != NULL) {
        free(fs->inodes);
        fs->inodes = NULL;
    }

    INODES *inodes = calloc(1, sizeof(INODES));

    fseek(fs->FILE, fs->superblock->inode_start_address, SEEK_SET);
    fread(inodes, sizeof(INODES), 1, fs->FILE);

    fs->inodes = inodes;
}

/**
 * Vypíše informace o i-nodu.
 *
 * @param inode
 */
void print_inode(PSEUDO_INODE *inode) {
    printf("Id: %d, ", inode->node_id);
    printf("Pa_id: %d, ", inode->parent_id);
    if (inode->is_free == true) {
        printf("free: true, ");
    } else {
        printf("free: false, ");
    }
    if (inode->isDirectory == true) {
        printf("dir: true, ");
    } else {
        printf("dir: false, ");
    }

    if (inode->isSLink == true) {
        printf("slink: true, ");
        printf("linked_file_id: %d, ", inode->linked_node_id);
    } else {
        printf("slink: false, ");
    }

    printf("fs: %ldB, ", inode->file_size);
    printf("cs: %d, ", inode->count_clusters);
    for (int i = 0; i < COUNT_DIRECT_LINK; i++) {
        printf("di: %d, ", inode->directs[i]);
    }
    printf("ind: %d, ", inode->indirect1);
    printf("ind: %d \n", inode->indirect2);
}

/**
 * Vypíše strukturu i-nodes.
 *
 * @param inodes
 */
void print_inodes(INODES *inodes) {
    printf("--- INODES ---\n");
    printf("Inodes size: %d\n", inodes->size);
    for (int i = 0; i < inodes->size; ++i) {
        print_inode(&inodes->data[i]);
    }
    printf("\n");
}

/**
 * Zpracuje cestu k hledanému i-node, najde i-node a vrátí jej, pokud vyhovuje hledanému typu.
 *
 * @param fs - struktura file systému
 * @param path - cesta k i-nodu (souboru)
 * @param type 0 = file, 1 = directory, 2 = obojí
 *
 * @return hledaný inode
 */
PSEUDO_INODE *get_inode(FS *fs, char *path, int32_t type) {
    PSEUDO_INODE *inode = NULL;

    // type incorrect
    if (type != 0 && type != 1 && type != 2) {
        printf("Error: incorrect type of inode.\n");
        return NULL;
    }

    // path is null
    if (path == NULL || path[0] == 0 || strlen(path) == 0) {
        if (type == 0) {
            printf("FILE NOT FOUND\n");
        } else if (type == 1) {
            printf("PATH NOT FOUND\n");
        } else {
            printf("SOURCE NOT FOUND\n");
        }
        return NULL;
    } else {
        // path is not null
        if (path[strlen(path) - 1] == '\n') {
            path[strlen(path) - 1] = '\0';
        }
        char tmp_path[PATH_MAX];
        strcpy(tmp_path, path);

        // path starts with ./
        if (tmp_path[0] == '.' && tmp_path[1] == '/') {
            int index = 0;
            for (int i = 2; i < strlen(tmp_path); ++i) {
                path[index] = tmp_path[i];

                if ((i + 1) == strlen(tmp_path)) {
                    path[index + 1] = '\0';
                }
                index++;
            }
        }

        // type 1 - directory
        if (type == 1 && path != NULL && path[strlen(path) - 2] == '/') {
            path[strlen(path) - 2] = '\0';
        } else if (type == 0 && path != NULL && path[strlen(path) - 1] == '/') {        // type 0 - file
            printf("FILE NOT FOUND\n");
            return NULL;
        }
        if (path != NULL && path[strlen(path) - 1] == '\n') {
            path[strlen(path) - 1] = '\0';
        }
    }

    // is path absolute or relative
    if (is_absolute_path(path) == true) {
        // starting search from root
        inode = search_for_inode(fs, &(fs->inodes->data[0]), path, false);
    } else {
        // starting search from current node
        inode = search_for_inode(fs, fs->current_inode, path, true);
    }

    // i-node was not found
    if (inode == NULL) {
        if (type == 0) {
            printf("FILE NOT FOUND\n");
        } else if (type == 1) {
            printf("PATH NOT FOUND\n");
        } else {
            printf("SOURCE NOT FOUND\n");
        }
        return NULL;
    }

    // found incorrect type
    if (inode->isDirectory == false && type == 1) {
        printf("DESTINATION NODE IS NOT DIRECTORY\n");
        return NULL;
    } else if (inode->isDirectory == true && type == 0) {
        printf("DESTINATION NODE IS NOT FILE\n");
        return NULL;
    }

    return inode;
}

/**
 * Najde i-node podle zadané cesty a vrátí jej.
 *
 * @param fs - struktura file systému
 * @param start_inode - inode, odkud začínáme hedat
 * @param path - cesta k souboru
 * @param search_from_current_node - true - hledáme od aktuálního adresáře, false - hledáme odjinud
 *
 * @return hledaný i-node
 */
PSEUDO_INODE *search_for_inode(FS *fs, PSEUDO_INODE *start_inode, char *path, bool search_from_current_node) {

    char *temp_path = calloc(strlen(path) + 1, sizeof(path));
    strcpy(temp_path, path);

    // reference to current directory
    DIRECTORY_ITEMS *current_directory = NULL;

    PSEUDO_INODE *current_inode = NULL;
    char *name_file;
    char temp_name_file[3];

    // should we search in current directory, or do we need to load whole structure
    if (search_from_current_node == true) {
        current_directory = fs->current_directory;
    } else {
        current_directory = read_directory_items_from_file(fs, start_inode);
    }

    // get first part of the path
    name_file = strtok(temp_path, "/");

    // path does not contain more '/'
    if (contains_char(temp_path, '/') == false) {
        strncpy(temp_name_file, name_file, 2);
        temp_name_file[2] = '\0';

        // path is parent i-node
        if (strcmp(temp_name_file, "..") == 0) {
            INODES *inodes = fs->inodes;
            current_inode = &inodes->data[start_inode->parent_id];
            return current_inode;
        }
        strncpy(temp_name_file, name_file, 1);
        temp_name_file[1] = '\0';

        // path is current i-node
        if (strcmp(temp_name_file, ".") == 0) {
            current_inode = fs->current_inode;
            return current_inode;
        }
    }

    bool found = false;
    int index = 0;

    while (name_file != NULL) {
        if (strlen(name_file) > 0) {
            if (current_inode != NULL && current_inode->isDirectory == false) {
                return NULL;
            }

            while (found == false && current_directory->size > index) {
                if (are_strings_equal(current_directory->data[index].item_name, name_file) == true) {

                    INODES *inodes = fs->inodes;
                    current_inode = &inodes->data[current_directory->data[index].node_id];

                    if (current_inode->isDirectory == true) {       // i-node is directory
                        current_directory = read_directory_items_from_file(fs, current_inode);
                        found = true;
                    } else {        // i-node is file
                        current_directory = NULL;
                        found = true;
                    }
                }
                index++;
            }

            // node not found
            if (found == false) {
                return NULL;
            }
            found = false;
            index = 0;
        }

        // get another part of the path
        name_file = strtok(NULL, "/");
    }

    return current_inode;
}

/**
 * Porovnává dva stringy.
 *
 * @param string1
 * @param string2
 * @return  true - shodují se
 *          false - neshodují se
 */
bool are_strings_equal(char *string1, char *string2) {
    char s1[strlen(string1)];
    char s2[strlen(string2)];

    strcpy(s1, string1);
    strcpy(s2, string2);

    if (strlen(s1) > 0 && s1[strlen(s1) - 1] == '\n') s1[strlen(s1) - 1] = '\0';
    if (strlen(s2) > 0 && s2[strlen(s2) - 1] == '\n') s2[strlen(s2) - 1] = '\0';

    if (strlen(s1) != strlen(s2)) {
        return false;
    }
    else {
        int i;
        for (i = 0; i < strlen(s1); i++) {
            if (s1[i] != s2[i]) {
                return false;
            }
        }
        return true;
    }
}

/**
 * Vrátí, zda se v řetězci vyskytuje znak.
 *
 * @param string - řetězec
 * @param pattern - znak
 * @return  true - ano
 *          false - ne
 */
bool contains_char(char *string, char pattern) {
    for (int i = 0; i < strlen(string); i++){
        if(string[0] == pattern) {
            return true;
        }
    }
    return  false;
}

/**
 * Vrátí poslední část cesty - filename (oddělené '/').
 *
 * @param path cesta
 *
 * @return pointer na filename
 */
char *get_filename_from_path(char *path) {
    char *name = calloc(FS_FILENAME_LENGTH, sizeof(char));
    char *temp_path_to_parent = calloc(PATH_MAX, sizeof(char));
    char *tmp = calloc(FS_FILENAME_LENGTH, sizeof(char));

    // path starts with ./
    if (path[0] == '.' && path[1] == '/') {
        int index = 2;
        for (int i = 0; i < strlen(path); ++i) {
            temp_path_to_parent[i] = path[index];
            index++;
        }
    } else {
        strcpy(temp_path_to_parent, path);
    }

    // does path contain more slashes?
    int slash_index = get_last_index_of_slash(temp_path_to_parent, '/');

    // there are more '/'
    if (slash_index != -1) {
        strcpy(tmp, temp_path_to_parent);
        tmp = strtok(tmp, "/");
        while (tmp != NULL) {
            strcpy(name, tmp);
            tmp = strtok(NULL, "/");
        }
    } else {
        strcpy(name, temp_path_to_parent);  // no '/' in path
    }

    if(name[strlen(name) - 1]  == '\n') {
        name[strlen(name) - 1] = '\0';
    }

    return name;
}

/**
 * Vrátí poslední index, kde se nachází znak '/'
 *
 * @param string řetězec
 * @return index
 */
int32_t get_last_index_of_slash(char *string, char pattern){
    int32_t index = -1;
    for (int i = 0; i < strlen(string); i++) {
        if(string[i] == '/'){
            index = i;
        }
    }
    return index;
}

/***
 * Onicializuje jeden i-node a vrátí jej.
 *
 * @param fs - struktura file systému
 * @param id_node - id i-nodu
 * @param parent_id - id rodiče
 * @param isFree - true - volný, false - není volný
 * @param isDirectory - true - složka, false - soubor
 * @param file_size  veliksot souboru
 * @param count_clusters pocet datových bloků
 * @param directs - pole přímých odkazů
 * @param indirect1  - 1. nepřímý odkaz
 * @param indirect2  - 2. nepřímý odkaz
 *
 * @return PSEUDO_INODE
 */
PSEUDO_INODE * init_pseudoinode(FS *fs, int32_t id_node, int32_t parent_id, bool isFree, bool isDirectory, int32_t file_size,
                 int32_t count_clusters, int32_t directs[COUNT_DIRECT_LINK], int32_t indirect1, int32_t indirect2) {
    INODES *inodes = fs->inodes;
    PSEUDO_INODE *inode = &inodes->data[id_node];
    inode->parent_id = parent_id;
    inode->isDirectory = isDirectory;
    inode->file_size = file_size;
    inode->is_free = isFree;
    inode->count_clusters = count_clusters;
    for (int j = 0; j < COUNT_DIRECT_LINK; j++) {
        inode->directs[j] = directs[j];
    }
    inode->indirect1 = indirect1;
    inode->indirect2 = indirect2;

    return inode;
}

/**
 * Zapíše obsah příslušných clusterů do souboru FS.
 *
 * @param fs - struktura file systému
 * @param new_inode - inode, jehož buffer chceme zapsat
 * @param clusters - datové bloky
 * @param buffer - data
 */
void write_clusters_to_file(FS *fs, PSEUDO_INODE *new_inode, int32_t *clusters, char **buffer) {

    fs->FILE = fopen(fs->filename, "rb+");
    int32_t actual_size = new_inode->file_size;

    // number of indirect links
    int32_t n_of_indirects = 0;
    if (new_inode->indirect1 != -1) {
        n_of_indirects++;
    }
    if (new_inode->indirect2 != -1) {
        n_of_indirects++;
    }

    // number of clusters that needs to use indirect links
    int32_t cluster_overflow = new_inode->count_clusters - COUNT_DIRECT_LINK;
    // how many int32 can go to one cluster
    int32_t n_of_ints_in_clusters = fs->superblock->cluster_size / sizeof(int32_t);

    // go through clusters
    for (int i = 0; i < new_inode->count_clusters; i++) {
        if (i < COUNT_DIRECT_LINK) {
            fseek(fs->FILE,fs->superblock->data_start_address + (new_inode->directs[i] * fs->superblock->cluster_size), SEEK_SET);
        } else {
            fseek(fs->FILE, fs->superblock->data_start_address + (clusters[i - COUNT_DIRECT_LINK]
                                                                  * fs->superblock->cluster_size), SEEK_SET);
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
        fwrite(clusters, sizeof(int32_t), cluster_overflow, fs->FILE);
    } else if (n_of_indirects == 2) {
        fseek(fs->FILE, fs->superblock->data_start_address + (new_inode->indirect1 * fs->superblock->cluster_size), SEEK_SET);
        fwrite(clusters, sizeof(int32_t), n_of_ints_in_clusters, fs->FILE);

        int32_t data_links[cluster_overflow - n_of_ints_in_clusters];
        int index = 0;
        for (int i = n_of_ints_in_clusters; i <= n_of_ints_in_clusters + (cluster_overflow - n_of_ints_in_clusters); ++i) {
            data_links[index] = clusters[i];
            index++;
        }

        fflush(fs->FILE);
        fseek(fs->FILE, fs->superblock->data_start_address + (new_inode->indirect2 * fs->superblock->cluster_size), SEEK_SET);

        fwrite(data_links, sizeof(int32_t), (cluster_overflow - n_of_ints_in_clusters), fs->FILE);
    }

    fflush(fs->FILE);

    // free memory
    for (int i = 0; i < new_inode->count_clusters; i++) {
        char *currentIntPtr = buffer[i];
        free(currentIntPtr);
    }
    free(buffer);
}

/**
 * Vrátí cestu k rodičovskému inodu.
 *
 * @param path aktuaální cesta
 *
 * @return cesta k rodiči
 */
char *get_path_to_parent(char *path){
    char *temp_path_to_parent = calloc(PATH_MAX, sizeof(char));

    if (path[0] == '.' && path[1] == '/') {
        int32_t index = 2;
        for (int i = 0; i < strlen(path); ++i) {
            temp_path_to_parent[i] = path[index];
            index++;
        }
    } else {
        strcpy(temp_path_to_parent, path);
    }

    int32_t last_index_forward_slash = get_last_index_of_slash(temp_path_to_parent, '/');
    // printf("cesta s poslednim indexem: %s - %d\n", temp_path_to_parent, last_index_forward_slash);
    if (last_index_forward_slash != -1) {
        for (int i = 0; i < strlen(temp_path_to_parent); i++) {
            if (i < last_index_forward_slash) {
                temp_path_to_parent[i] = temp_path_to_parent[i];
            } else if (i == last_index_forward_slash) {
                temp_path_to_parent[i] = '\0';
                break;
            }
        }

    }
    return temp_path_to_parent;
}

/**
 * Odstraní item (resp. i-node) z adresáře a zapíše změny do souboru FS.
 *
 * @param fs - struktura file systému
 * @param inode - i-node ke smazání
 * @return  true - smazání proběhlo úšpěšně
 *          false - jinak
 */
bool delete_inode(FS *fs, PSEUDO_INODE *inode) {
    int index = 0;
    bool result = false;

    fs->FILE = fopen(fs->filename, "rb+");

    // get parent
    INODES *inodes = fs->inodes;
    PSEUDO_INODE *parent_inode = &inodes->data[inode->parent_id];

    // load parent directory items
    DIRECTORY_ITEMS *parent_dir = read_directory_items_from_file(fs, parent_inode);

    DIRECTORY_ITEM *tmp_item = calloc(sizeof(DIRECTORY_ITEM), parent_dir->size);

    for (int j = 0; j < parent_dir->size; j++) {
        if (inode->node_id != parent_dir->data[j].node_id) {
            tmp_item[index].node_id = parent_dir->data[j].node_id;
            strcpy(tmp_item[index].item_name, parent_dir->data[j].item_name);
            index++;
            result = true;
        }
    }

    if (result == true) {
        parent_dir->size--;
        parent_dir->data = tmp_item;

        // write to file
        write_directory_items_to_file(fs, parent_dir, parent_inode);

        // free
        free_directory_items(parent_dir);
        parent_dir = NULL;
        free(tmp_item);
        tmp_item = NULL;
    } else {
        printf("Error deleting i-node.\n");
    }

    return result;
}

PSEUDO_INODE *get_parent_inode(FS *fs, PSEUDO_INODE *inode) {
    return &fs->inodes->data[inode->parent_id];
}

/**
 *  Inicializuje symbolický link.
 *
 * @param fs - struktura file systému
 * @param id_node - id i-nodu
 * @param parent_id - id rodičovského i-nodu
 * @param linked_id - id i-nodu, na který vytváříme link
 * @param directs - pole directs
 *
 * @return strukturu i-nodu
 */
PSEUDO_INODE * init_slink(FS *fs, int32_t id_node, int32_t parent_id, int32_t linked_id,
                          int32_t directs[COUNT_DIRECT_LINK]) {
    INODES *inodes = fs->inodes;
    PSEUDO_INODE *inode = &inodes->data[id_node];
    inode->parent_id = parent_id;
    inode->linked_node_id = linked_id;
    inode->isDirectory = false;
    inode->isSLink = true;
    inode->file_size = 0;
    inode->is_free = false;
    inode->count_clusters = 0;
    for (int j = 0; j < COUNT_DIRECT_LINK; j++) {
        inode->directs[j] = directs[j];
    }
    inode->indirect1 = -1;
    inode->indirect2 = -1;

    return inode;
}

