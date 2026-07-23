cmake_minimum_required(VERSION 3.20)
project(akai2sfz CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

add_compile_options(-Wall -Wextra)

add_library(akai2sfz_core
    src/codec.cpp
    src/image.cpp
    src/filesystem.cpp
)
target_include_directories(akai2sfz_core PUBLIC include)

add_executable(akai2sfz
    apps/cli/main.cpp
)
target_link_libraries(akai2sfz PRIVATE akai2sfz_core)

enable_testing()
add_subdirectory(tests)
