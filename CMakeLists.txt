cmake_minimum_required(VERSION 3.15)
project(145watch LANGUAGES CXX VERSION 1.5.0)
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_EXTENSIONS off)
if (MSVC)
    # vcpkg's toolchain only applies the triplet's CRT linkage to the ports
    # it builds, not to this project. Match it here, or linking a -static
    # triplet fails with LNK2038 (MT_StaticRelease vs MD_DynamicRelease).
    if(VCPKG_TARGET_TRIPLET MATCHES "static")
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    else()
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    endif()
    add_compile_options(/W3)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()
find_package(ftxui CONFIG REQUIRED)
find_package(argparse CONFIG REQUIRED)
add_executable(${PROJECT_NAME} main.cpp .clang-format README.md vcpkg.json)
target_compile_definitions(${PROJECT_NAME} PRIVATE
    WATCH_PROJECT_VERSION="${PROJECT_VERSION}"
)
target_link_libraries(${PROJECT_NAME} PRIVATE ftxui::dom ftxui::component ftxui::screen)
target_link_libraries(${PROJECT_NAME} PRIVATE argparse::argparse)
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE winmm)
endif()