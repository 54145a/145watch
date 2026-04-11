cmake_minimum_required(VERSION 3.10.0)
project(145watch LANGUAGES CXX VERSION 1.4.2)
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_EXTENSIONS off)

if (MSVC)
    add_compile_options(/W3)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

if(WIN32)
    find_package(unofficial-pdcurses CONFIG REQUIRED)
    add_library(Curses::Curses ALIAS unofficial::pdcurses::pdcurses)
else()
    find_package(Curses REQUIRED)
endif()

add_executable(${PROJECT_NAME} main.cpp)
target_compile_definitions(${PROJECT_NAME} PRIVATE
    WATCH_PROJECT_VERSION="${PROJECT_VERSION}"
)

target_link_libraries(${PROJECT_NAME} PRIVATE Curses::Curses)

if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE winmm)
endif()
