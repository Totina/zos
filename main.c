
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "header.h"
#include "fs.h"
#include "inodes.h"
#include "commands.h"
#include "directory.h"

#define SIGNATURE "toti"
#define DESCRIPTOR "inodes pseudo file system"
#define FILENAME "myFS"

int isRunning = 1;      // 1 = yes

int main(int argc, char *argv[]) {

    // handle argument - FS filename
    char name[FS_FILENAME_LENGTH];
    if(argc > 1) {
        strcpy(name, argv[1]);
    }
    else {
        strcpy(name, FILENAME);
    }

    // initialize FS
    FS *fs = NULL;
    fs = fs_init(name, SIGNATURE, DESCRIPTOR, DISK_SIZE, 0);

    // command from user
    char command[MAX_COMMAND_LENGTH];
    char *token;

    while (isRunning) {
        if (fs->actual_path[0] == 0) {
            printf("%s:%s%s%s", fs->superblock->signature, ROOT_CHAR, fs->actual_path, SHELL_CHAR);
        } else {
            printf("%s:%s%s", fs->superblock->signature, fs->actual_path, SHELL_CHAR);
        }

        // gets command from user
        fgets(command, MAX_COMMAND_LENGTH, stdin);

        // splits the arguments
        token = strtok(command, SPLIT_ARGS_CHAR);

        fs->FILE = fopen(fs->filename, "rb+");

        // sends command to the functions that handles it
        commands(fs, token);

        fclose(fs->FILE);
    }

    free(fs);
    return 0;
}

/**
 * Handles commands from user.
 *
 */
int commands(FS *fs, char *token) {
    // incp - nahraje soubor s1 z pevného disku do umístění s2 v pseudoNTFS
    if (strcmp(token, FILE_IN) == 0) {
        file_in(fs, token);
        update_current_directory(fs);
    }
    // ls - print directory
    else if (are_strings_equal(token, PRINT_DIRECTORY) == true) {
        print_directory(fs, token);
    }
    // cat - print file
    else if (strcmp(token, PRINT_FILE) == 0) {
        print_file(fs, token);
    }
    // mkdir - create directory
    else if (strcmp(token, MAKE_DIRECTORY) == 0) {
        make_directory(fs, token);
        update_current_directory(fs);
    }
    // rm - remove file
    else if (strcmp(token, REMOVE_FILE) == 0) {
        remove_file_or_directory(fs, token, false);
        update_current_directory(fs);
    }
    // rmdir - remove directory
    else if (strcmp(token, REMOVE_EMPTY_DIRECTORY) == 0) {
        remove_file_or_directory(fs, token, true);
        update_current_directory(fs);
    }
    // cp - copy file
    else if (strcmp(token, COPY_FILE) == 0) {
        copy_file(fs, token);
    }
    // mv - move file                       TO DO: change name
    else if (strcmp(token, MOVE_FILE) == 0) {
        move_file(fs, token);
    }
    // cd - change directory
    else if (strcmp(token, CHANGE_DIRECTORY) == 0) {
        change_directory(fs, token);
    }
    // pwd - print working directory
    else if (are_strings_equal(token, PRINT_WORKING_DIRECTORY) == true) {
        print_working_directory(fs);
    }
    // info - print info about file or directory
    else if (strcmp(token, INFO) == 0) {
        print_info(fs, token);
    }
    // outcp - nahraje soubor s1 z pseudoNTFS do umístění s2 na pevném disku
    else if (strcmp(token, FILE_OUT) == 0) {
        file_out(fs, token);
        update_current_directory(fs);
    }
    // load
    else if (strcmp(token, LOAD_COMMANDS) == 0) {
        printf("load\n");
        load_file_with_commands(fs, token);
        update_current_directory(fs);
    }
    // format
    else if (strcmp(token, FORMAT) == 0) {
        FS *new_fs = file_formatting(token, FILENAME, SIGNATURE, DESCRIPTOR);
        if (new_fs != NULL) {
            fs = new_fs;
        }
        update_current_directory(fs);
    }
    // slink - creates symbolic link
    else if(strcmp(token, S_LINK) == 0) {
        create_slink(fs, token);
        update_current_directory(fs);
    }
    // quit
    else if(are_strings_equal(token, QUIT) == true) {
        isRunning = 0;
    }
    // print FS
    else if(are_strings_equal(token, PRINT_FS) == true) {
        print_fs(fs);
    }
    // print help
    else if(are_strings_equal(token, HELP) == true) {
        //print_help(fs);
    }
    else {
        printf("Command not found.");
    }

}
