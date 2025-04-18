cmake_minimum_required(VERSION 3.28)
project(my-router-info-protocol)

set(CMAKE_C_COMPILER clang)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# GTK3 via pkg-config
find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK3 REQUIRED gtk+-3.0)

# Copy config files
file(GLOB RIPTBL_FILES "${CMAKE_SOURCE_DIR}/src/riptbls/*")
file(COPY ${RIPTBL_FILES}
    DESTINATION ${CMAKE_BINARY_DIR}
)
file(GLOB HOSTTBL_FILES "${CMAKE_SOURCE_DIR}/src/hosttbls/*")
file(COPY ${HOSTTBL_FILES}
    DESTINATION ${CMAKE_BINARY_DIR}
)


add_library(first STATIC
    src/first/first.c
)
target_include_directories(first PUBLIC
    src/first
)

add_library(host STATIC
    src/host/host.c
)
target_include_directories(host PUBLIC
    src/host
)
target_link_libraries(host PUBLIC
    first
)


# Executable
add_executable(peer-listen src/peer-listen/peer-listen.c)

# Target
target_include_directories(peer-listen PRIVATE
    ${GTK3_INCLUDE_DIRS}
)
target_compile_options(peer-listen PRIVATE
    ${GTK3_CFLAGS_OTHER}
)
target_link_libraries(peer-listen PRIVATE
    ${GTK3_LIBRARIES} 
    first
    host
)
