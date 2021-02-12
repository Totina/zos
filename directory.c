//
// Created by terez on 2/9/2021.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "directory.h"
#include "fs.h"
#include "inodes.h"

/**
 * Vytvoří directory item a inicializuje v něm dva další soubory typu directory item - sebe a odkaz na parenta.
 *
 * @param fs - struktura file systému
 * @param parent_node_id - ID i-nodu rodiče
 * @param inode - inode tohoto directory
 * @param name_directory - název této složky - directory_item
 *
 * @return vytvořenou strukturu directory_item
 */
DIRECTORY_ITEMS *create_directory_item(FS *fs, int32_t parent_node_id, PSEUDO_INODE *inode, char *name_directory) {
    // looks for number of clusters we need
    if (find_free_clusters(fs, 1) == false) {
        printf("Not enough free clusters.\n");
        return NULL;
    }

    // looks for free i-node
    if (find_free_node(fs) == false) {
        printf("No free I-nodes. \n");
        return NULL;
    }

    // allocation
    DIRECTORY_ITEMS *items = calloc(1, sizeof(DIRECTORY_ITEMS));
    items->size = 2;
    items->data = calloc(items->size, sizeof(DIRECTORY_ITEM));

    // add root
    strcpy(items->data[0].item_name, ".");
    items->data[0].node_id = inode->node_id;
    // add parent
    strcpy(items->data[1].item_name, "..");
    items->data[1].node_id = parent_node_id;

    inode->isDirectory = true;
    inode->is_free = false;
    inode->parent_id = parent_node_id;
    inode->file_size = sizeof(items) + items->size * sizeof(DIRECTORY_ITEM);

    // getting free clusters for diectory item
    assign_clusters(fs, inode);

    return items;
}

/**
 * Vrátí, jestli je volný potřebný počet clusterů.
 *
 * @param fs - struktura file systému
 * @param count - potřebný počet clusterů
 *
 * @return  true, pokud je ve FS potřebný počet volných clusterů
 *          false, pokud nená
 */
bool find_free_clusters(FS *fs, int32_t count){
    // number of free clusters found
    int n_of_clusters_found = 0;

    for (int i = 0; i < fs->bitmap->size; ++i) {
        if(fs->bitmap->cluster_free[i] == true){
            n_of_clusters_found++;
        }
        if(n_of_clusters_found >= count) {
            return true;
        }
    }

    return false;
}

/**
 * Vrátí jestli je nějaký i-node volný.
 *
 * @param fs - struktura file systému
 *
 * @return  true, pokud takový i-node existuje
 *          false, pokud ne
 */
bool find_free_node(FS *fs) {
    for (int i = 0; i < fs->inodes->size; ++i) {
        if(fs->inodes->data[i].is_free == true) {
            return true;
        }
    }
    return false;
}

/**
 * Zapíše do clusterů strukturu directory_items do souboru file systému.
 *
 * @param fs - struktura file systému
 * @param items directory_items
 * @param inode inode příslušného datového bloku
 */
void write_directory_items_to_file(FS *fs, DIRECTORY_ITEMS *items, PSEUDO_INODE *inode) {
    int index = 0;
    int item_size = 0;

    // number of directory items in one cluster
    int dirs_in_cluster = fs->superblock->cluster_size / sizeof(DIRECTORY_ITEM);

    fseek(fs->FILE, fs->superblock->data_start_address + inode->directs[0] * fs->superblock->cluster_size, SEEK_SET);
    fwrite(items, sizeof(DIRECTORY_ITEMS), 1, fs->FILE);
    fflush(fs->FILE);

    DIRECTORY_ITEM *data = calloc(dirs_in_cluster, sizeof(DIRECTORY_ITEM));

    for (int i = 0; i < inode->count_clusters; i++) {
        // copy item to data
        if (i == 0) {
            // v prvnim clusteru je tato DIRECTORY_ITEMS
            fseek(fs->FILE, fs->superblock->data_start_address + inode->directs[i] * fs->superblock->cluster_size +
                            sizeof(DIRECTORY_ITEMS), SEEK_SET);
        } else {
            // jinak se presunu co jiného clusteru
            fseek(fs->FILE, fs->superblock->data_start_address + inode->directs[i] * fs->superblock->cluster_size,
                  SEEK_SET);
        }

        // data to write
        for (int j = 0; j < dirs_in_cluster; j++) {
            data[j] = items->data[index];
            index++;
            item_size++;
            if (j + 1 == items->size) {
                break;
            }
        }

        fwrite(data, sizeof(DIRECTORY_ITEM), item_size, fs->FILE);

        item_size = 0;
    }

    fflush(fs->FILE);
    if (inode->count_clusters > COUNT_DIRECT_LINK) {
        printf("Not enough clusters for directory item. \n");
    }

}

/**
 * Přečte ze souboru file systému strukturu directory_items naplněnou příslušnými soubory.
 *
 * @param fs - struktura file systému
 * @param inode - i-node directory item, který čteme
 *
 * @return přečtené directory_items ze souboru
 */
DIRECTORY_ITEMS *read_directory_items_from_file(FS *fs, PSEUDO_INODE *inode) {
    int index = 0;

    // number of directory items in one cluster
    int dirs_in_cluster = fs->superblock->cluster_size / sizeof(DIRECTORY_ITEM);

    DIRECTORY_ITEMS *directory_items = calloc(1, sizeof(DIRECTORY_ITEMS));

    fseek(fs->FILE, fs->superblock->data_start_address + inode->directs[0] * fs->superblock->cluster_size, SEEK_SET);
    fread(directory_items, sizeof(DIRECTORY_ITEMS), 1, fs->FILE);

    directory_items->data = calloc(directory_items->size, sizeof(DIRECTORY_ITEM));
    /*if( directory_items->data  == NULL){
        printf("Error: nedostatek pameti(2) \n");
        return NULL;
    }*/
    DIRECTORY_ITEM *data = calloc(dirs_in_cluster, sizeof(DIRECTORY_ITEM));

    for (int i = 0; i < inode->count_clusters; i++) {
        if (i == 0) {
            // v prvnim clusteru je tako DIRECTORY_ITEMS
            fseek(fs->FILE, fs->superblock->data_start_address + inode->directs[i] * fs->superblock->cluster_size +
                            sizeof(DIRECTORY_ITEMS), SEEK_SET);
        } else {
            // jinak se presunu co jiného clusteru
            fseek(fs->FILE, fs->superblock->data_start_address + inode->directs[i] * fs->superblock->cluster_size, SEEK_SET);
        }

        fread(data, sizeof(DIRECTORY_ITEM), dirs_in_cluster, fs->FILE);

        // data to read
        for (int j = 0; j < directory_items->size; j++) {
            strcpy(directory_items->data[index].item_name, data[j].item_name);
            directory_items->data[index].node_id = data[j].node_id;
            index++;
        }
    }

    free(data);

    return directory_items;
}

/**
 * Funkce, která vypéše celou naplněno ustrukturu directory_items
 * @param items directory_items
 */
void print_directory_items(DIRECTORY_ITEMS *items) {
    printf("------------------------\n");
    printf("Directory size: %d \n", items->size);
    for (int i = 0; i < items->size; i++) {
        print_directory_item(&items->data[i]);
    }
    printf("------------------------\n");
}

/**
 * Funkce, která vypíše položku v directory_items
 * @param item directory_item
 */
static void print_directory_item(DIRECTORY_ITEM *item) {
    printf("%s (node ID: %d)\n", item->item_name, item->node_id);
}

/**
 * Vytvoří soubor v našem sytému, který bude obsahovat data, která jsou v souboru source_file
 *
 * @param fs - struktura file systému
 * @param source_file soubor, který chceme přesunout do našeh systému
 * @param filename  jméno souboru
 * @param dest_inode - inod, představující diretcory_item do kterého chceme soubor vytvořit
 * @return true - povedlo-li se, naopak je false
 */
bool create_file_in_FS(FS *fs, FILE *source_file, char *filename, PSEUDO_INODE *dest_inode) {
    // get file size
    fseek(source_file, 0, SEEK_END);
    int file_size = ftell(source_file);
    fseek(source_file, 0, SEEK_SET);

    // reference to destination directory
    DIRECTORY_ITEMS *dest_directory = read_directory_items_from_file(fs, dest_inode);

    // does directory already contains file with this name?
    if (directory_contains_file(dest_directory, filename) == true) {
        printf("File or directory '%s' already exists.\n", filename);
        fclose(source_file);
        return false;
    }

    // get free i-node
    PSEUDO_INODE *new_inode = get_free_inode(fs);
    if (new_inode == NULL) {
        printf("NO FREE I-NODE FOUND\n");
        fclose(source_file);
        return false;
    }

    // how many clusters we need
    int n_of_clusters = 1;
    if (file_size != 0) {
        n_of_clusters = file_size / fs->superblock->cluster_size;
        if (file_size % CLUSTER_SIZE != 0) {    // not a full cluster
            n_of_clusters++;
        }
    }

    // number of clusters that needs to use indirect links
    int32_t cluster_overflow = 0;
    // number of indirect links
    int32_t n_of_indirects = 0;
    // how many int32 can go to one cluster
    int32_t n_of_ints_in_cluster = fs->superblock->cluster_size / sizeof(int32_t);

    if (n_of_clusters > COUNT_DIRECT_LINK) {
        cluster_overflow = n_of_clusters - COUNT_DIRECT_LINK;
        n_of_indirects = cluster_overflow / n_of_ints_in_cluster;
        if (cluster_overflow % n_of_ints_in_cluster != 0) {
            n_of_indirects++;
        }
    }

    // number of indirect links too big
    if (n_of_indirects > 2) {
        printf("FILE IS TOO BIG, NOT ENOUGH INDIRECT LINKS.\n");
        fclose(source_file);
        return false;
    }

    // find free clusters
    if (find_free_clusters(fs, n_of_clusters + n_of_indirects) == false) {
        printf("NOT ENOUGH FREE CLUSTERS\n");
        fclose(source_file);
        return false;
    }

    int32_t read_size = fs->superblock->cluster_size;

    // adds file to the directory
    add_item_to_directory(fs, dest_directory, dest_inode, filename, new_inode);

    // initialize new i-node
    int32_t *directs = malloc(sizeof(int32_t)* COUNT_DIRECT_LINK);
    for (int i = 0; i < COUNT_DIRECT_LINK; i++) {
        if (i >= n_of_clusters) {
            directs[i] = -1;
        } else {
            directs[i] = get_cluster(fs);
            fs->bitmap->cluster_free[directs[i]] = false;
        }
    }

    int32_t indirect1 = -1;
    int32_t indirect2 = -1;

    if (n_of_indirects == 1) {
        indirect1 = get_cluster(fs);
        fs->bitmap->cluster_free[indirect1] = false;
    } else if (n_of_indirects == 2) {
        indirect1 = get_cluster(fs);
        fs->bitmap->cluster_free[indirect1] = false;
        indirect2 = get_cluster(fs);
        fs->bitmap->cluster_free[indirect2] = false;
    }

    // initialize new i-node
    new_inode = init_pseudoinode(fs, new_inode->node_id, dest_inode->node_id, false, false, file_size,
                                 n_of_clusters, directs, indirect1, indirect2);

    // indirect links
    int32_t data_links[cluster_overflow];
    for (int j = 0; j < cluster_overflow; j++) {
        data_links[j] = get_cluster(fs);
        fs->bitmap->cluster_free[data_links[j]] = false;
    }

    char **buffer = malloc(n_of_clusters * sizeof(char *));
    for (int i = 0; i < n_of_clusters; ++i) {
        buffer[i] = (char *) malloc((fs->superblock->cluster_size + 1) * sizeof(char));
    }

    // data
    size_t offset = 0;
    long actual_size = file_size;
    for (int i = 0; i < n_of_clusters; i++) {
        offset = i * fs->superblock->cluster_size;
        fseek(source_file, offset, SEEK_SET);
        fread(buffer[i], sizeof(char), read_size, source_file);
        if (i == (n_of_clusters - 1)) {
            buffer[i][actual_size] = '\0';
        } else {
            buffer[i][read_size] = '\0';
        }
        actual_size -= fs->superblock->cluster_size;
    }

    // write changes to file
    write_clusters_to_file(fs, new_inode, data_links, buffer);
    write_inodes_to_file(fs);
    write_bitmap_to_file(fs);

    fclose(source_file);
    return true;
}

/**
 *
 *
 * @param fs
 * @param source_file
 * @param filename
 * @param dest_inode
 * @return
 */
bool create_s_link(FS *fs, char *filename, char *linked_file_name, PSEUDO_INODE *dest_inode) {

    // reference to destination directory
    DIRECTORY_ITEMS *dest_directory = read_directory_items_from_file(fs, dest_inode);

    // does directory already contains file with this name?
    if (directory_contains_file(dest_directory, filename) == true) {
        printf("File or directory '%s' already exists.\n", filename);
        return false;
    }

    // linked file node ID
    int32_t linked_file_node_id;
    for (int i = 0; i < dest_directory->size; ++i) {
        if (strcmp(dest_directory->data[i].item_name, linked_file_name) == 0) {
            linked_file_node_id = dest_directory->data[i].node_id;
            printf("Found node ID %d\n", linked_file_node_id);
        }
    }

    // get free i-node
    PSEUDO_INODE *new_inode = get_free_inode(fs);
    if (new_inode == NULL) {
        printf("NO FREE I-NODE FOUND\n");
        return false;
    }

    // adds file to the directory
    add_item_to_directory(fs, dest_directory, dest_inode, filename, new_inode);

    int n_of_clusters = 0;

    // initialize new i-node
    int32_t *directs = malloc(sizeof(int32_t)* COUNT_DIRECT_LINK);
    for (int i = 0; i < COUNT_DIRECT_LINK; i++) {
            directs[i] = -1;
    }


    // initialize new i-node
    new_inode = init_slink(fs, new_inode->node_id, dest_inode->node_id, linked_file_node_id, directs);

    // write changes to file
    //write_clusters_to_file(fs, new_inode, data_links, buffer);
    write_inodes_to_file(fs);
    write_bitmap_to_file(fs);

    return true;

}

/**
 * Vrátí, zda se soubor se stejným jménem již v adresáři nachází.
 *
 * @param items directory item (adresář), který prohlížíme
 * @param filename jméno souboru
 *
 * @return  true - soubor s tímto jménem již existuje
 *          false - soubor neexistuje
 */
bool directory_contains_file(DIRECTORY_ITEMS *items, char *filename) {
    for (int i = 0; i < items->size; ++i) {
        if (strcmp(items->data[i].item_name, filename) == 0) {
            return true;
        }
    }
    return false;
}

/**
 * Přidá item (soubor) do directory item a zapíše jej do souboru FS.
 *
 * @param fs - struktura file systému
 * @param items - directory item, do kterého přidávám nový item
 * @param inode - i-node itemu
 * @param name - název nového itemu
 * @param new_inode - i-node představující nový item
 */
void add_item_to_directory(FS *fs, DIRECTORY_ITEMS *items, PSEUDO_INODE *inode, char *name, PSEUDO_INODE *new_inode) {
    fs->FILE = fopen(fs->filename, "rb+");

    // set parent id of the new i-node to current i-node
    new_inode->parent_id = inode->node_id;

    // increment size
    items->size = items->size + 1;

    // realloc memory, so there is memory for the new item
    items->data = realloc(items->data, items->size * sizeof(DIRECTORY_ITEM));

    // copy data
    strcpy(items->data[items->size - 1].item_name, name);
    items->data[(items->size - 1)].node_id = new_inode->node_id;

    inode->file_size = sizeof(DIRECTORY_ITEMS) + items->size * sizeof(DIRECTORY_ITEM);

    // write to file
    write_directory_items_to_file(fs, items, inode);
}

/**
 * Funkce, která uvolnuje pamět alokovanému polpoly v directory_item
 *
 * @param items pole directory_items, které chceme uvolnit
 */
void free_directory_items(DIRECTORY_ITEMS *items){
    free(items);
}

/**
 * Vrátí pole odkazů na všechny clustery, ve kterých je zapsán soubor.
 *
 * @param fs - struktura file systému
 * @param inode - i-node souboru
 *
 * @return  pole odkazů na datové bloky
 */
int32_t *get_all_file_clusters(FS *fs, PSEUDO_INODE *inode) {
    int index = 0;

    // allocation of all file clusters
    int32_t *file_clusters = (int32_t*)malloc(sizeof(int32_t) * inode->count_clusters);

    // direct links
    for (int j = 0; j < inode->count_clusters; j++) {
        if (inode->directs[j] != -1 && j < COUNT_DIRECT_LINK) {   // j + 1 <= COUNT_DIRECT_LINK
            file_clusters[j] = inode->directs[j];
            index++;
        } else {
            file_clusters[j] = -1;
        }
    }

    int32_t *data1 = (int32_t*)malloc(sizeof(int32_t) * inode->count_clusters);

    // first indirect link is used
    if (inode->indirect1 != -1) {
        fseek(fs->FILE, fs->superblock->data_start_address + (inode->indirect1 * fs->superblock->cluster_size), SEEK_SET);

        int count = 0;
        if(inode->indirect2 == -1) {
            count = (inode->count_clusters - COUNT_DIRECT_LINK);
        } else {
            count = fs->superblock->cluster_size / sizeof(int32_t);
        }

        fread(data1, sizeof(int32_t), count, fs->FILE);

        for (int i = 0; i < count; i++) {
            file_clusters[index] = data1[i];
            index++;
        }
    }

    // second indirect link is used
    if (inode->indirect2 != -1) {
        fseek(fs->FILE, fs->superblock->data_start_address + (inode->indirect2 * fs->superblock->cluster_size), SEEK_SET);

        int32_t count = 0;
        count = (inode->count_clusters - COUNT_DIRECT_LINK - (fs->superblock->cluster_size / sizeof(int32_t)));

        int32_t data2[count];

        fread(data2, sizeof(int32_t), count, fs->FILE);

        for (int i = 0; i < count; i++) {
            file_clusters[index] = data2[i];
            index++;
        }
    }

    return file_clusters;
}