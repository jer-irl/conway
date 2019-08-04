project(conway)
cmake_minimum_required(VERSION 3.7)

add_executable(conway
        conway.c
        )

target_link_libraries(conway
        ncurses
        )

set_target_properties(conway PROPERTIES C_STANDARD 11)
