//
// Created by terez on 2/10/2021.
//

#ifndef ZOS_COMMANDS_H
#define ZOS_COMMANDS_H


void file_in(FS *fs, char *token);
void print_directory(FS *fs, char *path);
void print_file(FS *fs, char *path);
void make_directory(FS *fs, char *path);
void remove_file_or_directory(FS *fs, char *path, bool isDirectory);
void copy_file(FS *fs, char *token);
void move_file(FS *fs, char *token);
void change_directory(FS *fs, char *path);
void print_working_directory(FS *fs);
void print_info(FS *fs, char *path);
void file_out(FS *fs, char *token);
void load_file_with_commands(FS *fs, char *token);
FS *format_fs(char *token, char *filename, char *signature, char *descriptor);

int handle_bytes(char *size, int size_digits);
int index_of_last_digit(char *number);

void create_slink(FS *fs, char *path);

void print_help();


#endif //ZOS_COMMANDS_H
