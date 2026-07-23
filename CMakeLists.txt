cmake_minimum_required(VERSION 3.20)
project(akai2sfz VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

add_compile_options(-Wall -Wextra)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SNDFILE REQUIRED sndfile)

add_library(akai2sfz_core
    src/codec.cpp
    src/image.cpp
    src/cd_container.cpp
    src/filesystem.cpp
    src/akai_format.cpp
    src/wav_writer.cpp
    src/sfz_writer.cpp
    src/converter.cpp
    src/roland_filesystem.cpp
    src/roland_format.cpp
    src/roland_converter.cpp
    src/emu_filesystem.cpp
    src/emu_format.cpp
    src/emu_converter.cpp
    src/kurzweil_filesystem.cpp
    src/krz_format.cpp
    src/krz_converter.cpp
)
target_include_directories(akai2sfz_core PUBLIC include)
target_include_directories(akai2sfz_core PRIVATE ${SNDFILE_INCLUDE_DIRS})
target_link_libraries(akai2sfz_core PUBLIC ${SNDFILE_LIBRARIES})
target_link_directories(akai2sfz_core PUBLIC ${SNDFILE_LIBRARY_DIRS})

add_executable(akai2sfz
    apps/cli/main.cpp
)
target_link_libraries(akai2sfz PRIVATE akai2sfz_core)
install(TARGETS akai2sfz RUNTIME DESTINATION bin)

enable_testing()
add_subdirectory(tests)

option(AKAI2SFZ_BUILD_GUI "Build the Qt6 GUI app" ON)
if(AKAI2SFZ_BUILD_GUI)
  find_package(Qt6 COMPONENTS Widgets)
  if(Qt6_FOUND)
    add_subdirectory(apps/gui)
  else()
    message(WARNING "Qt6 nao encontrado -- pulando o build da GUI (WJ-VSC)")
  endif()
endif()
