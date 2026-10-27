
cmake_minimum_required(VERSION 3.20)

project(VoxelCraft LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Reconfigure automatically when source files are added or removed.
file(GLOB_RECURSE VOXELCRAFT_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/glad/src/*.c"
)

file(GLOB_RECURSE VOXELCRAFT_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/glad/include/*.h"
)

find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)
find_package(PNG REQUIRED)

add_executable(VoxelCraft
    ${VOXELCRAFT_SOURCES}
    ${VOXELCRAFT_HEADERS}
)

target_include_directories(VoxelCraft PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/glad/include"
)

target_link_libraries(VoxelCraft PRIVATE
    glfw
    glm
    PNG::PNG
)

# Distribution layout:
# dist/
#   VoxelCraft
#   lib/
#
# The executable loads bundled libraries from dist/lib.
set(DIST_DIR "${CMAKE_BINARY_DIR}/../dist")
set(DIST_LIB_DIR "${DIST_DIR}/lib")

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set_target_properties(VoxelCraft PROPERTIES
        BUILD_RPATH "$ORIGIN/../lib"
        INSTALL_RPATH "$ORIGIN/lib"
    )

    # Copy the executable and all resolved non-system runtime libraries
    # after each successful build.
    add_custom_command(TARGET VoxelCraft POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E make_directory
            "${DIST_LIB_DIR}"
        COMMAND "${CMAKE_COMMAND}" -E copy
            "$<TARGET_FILE:VoxelCraft>"
            "${DIST_DIR}/VoxelCraft"
        COMMAND "${CMAKE_COMMAND}" -E env
            "VOXELCRAFT_BINARY=$<TARGET_FILE:VoxelCraft>"
            "VOXELCRAFT_DIST_LIB=${DIST_LIB_DIR}"
            sh -c
            "ldd \"$VOXELCRAFT_BINARY\" | awk '/=> \\/.*\\.so/ {print $3} /^[[:space:]]*\\// {print $1}' | sort -u | while read -r lib; do case \"$lib\" in /usr/lib/libc.so*|/usr/lib/libm.so*|/usr/lib/ld-linux*|/lib64/ld-linux*) continue ;; esac; [ -f \"$lib\" ] && cp -L -n \"$lib\" \"$VOXELCRAFT_DIST_LIB/\"; done"
        COMMAND patchelf --set-rpath "$ORIGIN/lib"
            "${DIST_DIR}/VoxelCraft"
        VERBATIM
    )
endif()
