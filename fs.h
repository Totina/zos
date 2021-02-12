//
// Created by terez on 2/9/2021.
//

#ifndef ZOS_FS_H
#define ZOS_FS_H


FS *fs_init(char *filename, char *signature, char *descriptor, size_t disk_size, int format);
void create_file(FS *fs);
void load_fs_from_file(FS *fs);
SUPERBLOCK *superblock_init(char *signature, char *volume_descriptor, int32_t disk_size, int32_t cluster_size);
BITMAP *bitmap_init(int32_t cluster_count);

void write_superblock_to_file(FS *fs);
void write_bitmap_to_file(FS *fs);
void update_current_directory(FS *fs);
void read_sb_from_file(FS *fs);
void read_bitmap_from_file(FS *fs);

void print_fs(FS *fs);
void print_superblock(SUPERBLOCK *superblock);
void print_bitmap(BITMAP *bitmap);

void set_path_to_root(FS *fs);
char *get_absolute_path(FS *fs, char *path);


#endif //ZOS_FS_H
