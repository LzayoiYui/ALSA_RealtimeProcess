
cmake_minimum_required(VERSION 3.14)
project(ALSA_RealtimeProcess LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(ALSA REQUIRED)

# Library (core)
add_library(arp_core
    src/alsa_capture.cpp
    src/alsa_playback.cpp
)

target_include_directories(arp_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(arp_core PUBLIC ALSA::ALSA)

# Examples
add_executable(arp_record examples/record.cpp)
target_link_libraries(arp_record PRIVATE arp_core)

add_executable(arp_playback examples/playback.cpp)
target_link_libraries(arp_playback PRIVATE arp_core)

add_executable(arp_duplex examples/duplex_main.cpp)
target_link_libraries(arp_duplex PRIVATE arp_core)

# Warnings
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(arp_core PRIVATE -Wall -Wextra)
    foreach(tgt arp_record arp_playback arp_duplex)
        target_compile_options(${tgt} PRIVATE -Wall -Wextra)
    endforeach()
endif()

# Install
include(GNUInstallDirs)
install(TARGETS arp_core arp_record arp_playback arp_duplex
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
