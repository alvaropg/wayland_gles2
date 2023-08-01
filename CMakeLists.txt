cmake_minimum_required(VERSION 2.8.7)
project(wayland_egl C CXX)

include_directories(${CMAKE_SOURCE_DIR})
include_directories(${CMAKE_SOURCE_DIR}/src)
include_directories(${CMAKE_SOURCE_DIR}/include)

include(FindPkgConfig)
include(GNUInstallDirs)

pkg_check_modules(WLCLIENT REQUIRED wayland-client)
include_directories(${WLCLIENT_INCLUDE_DIRS})

pkg_check_modules(WLEGL REQUIRED wayland-egl)
include_directories(${WLEGL_INCLUDE_DIRS})

pkg_check_modules(GLESV2 REQUIRED glesv2)
include_directories(${GLESV2_INCLUDE_DIRS})

set(BIN_NAME wayland_egl)

set(SRC_LIST
        ${CMAKE_SOURCE_DIR}/src/init_window.c
)

#set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/pkg_$ENV{ARCH}/")
add_executable(${BIN_NAME} ${SRC_LIST})
set_target_properties(${BIN_NAME} PROPERTIES LINKER_LANGUAGE C)

target_link_libraries (${BIN_NAME}
        ${WLCLIENT_LDFLAGS}
        ${WLEGL_LDFLAGS}
        -lEGL
        ${GLESV2_LDFLAGS}
        -lm
)

install(TARGETS ${BIN_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR})
