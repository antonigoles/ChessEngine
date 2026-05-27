cmake_minimum_required(VERSION 3.25)
project(ChessBot VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

file(GLOB_RECURSE SOURCES "src/*.cpp")

add_executable(${PROJECT_NAME} ${SOURCES})
target_include_directories(${PROJECT_NAME} PRIVATE src)

target_compile_options(${PROJECT_NAME} PRIVATE)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Konfiguracja profilu: DEBUG (z flagami fsanitize)")
    target_compile_options(${PROJECT_NAME} PRIVATE -fsanitize=address,undefined -march=native -O3)
    target_link_options(${PROJECT_NAME} PRIVATE -fsanitize=address,undefined)
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    message(STATUS "Konfiguracja profilu: PRODUCTION/RELEASE (-march=native)")
    target_compile_options(${PROJECT_NAME} PRIVATE -march=native -O3)
endif()