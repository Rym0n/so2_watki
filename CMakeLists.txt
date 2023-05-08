cmake_minimum_required(VERSION 3.16)
project(so2_watki C)

set(CMAKE_C_STANDARD 99)

add_link_options(-pthread)
add_link_options(-lncursesw)

add_executable(so2_watki main.c functions.h functions.c)

target_link_libraries(so2_watki ncurses)
target_link_libraries(so2_watki rt)