cmake_minimum_required(VERSION 3.18)
project(ZOS C)

set(CMAKE_C_STANDARD 99)

add_executable(ZOS main.c header.h fs.c fs.h inodes.c inodes.h directory.c directory.h commands.c commands.h)