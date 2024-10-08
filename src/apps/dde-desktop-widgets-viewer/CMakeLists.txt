cmake_minimum_required(VERSION 3.10)
set(BIN_NAME "dde-desktop-widgets-viewer")

set(CMAKE_INCLUDE_CURRENT_DIR ON)
find_package(Qt6 REQUIRED COMPONENTS Core)

FILE(GLOB_RECURSE SRCS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    )

add_executable(${BIN_NAME}
    ${SRCS}
)

target_link_libraries(${BIN_NAME} PRIVATE
    Qt6::Core
    dde-desktop-widgets
)

install(TARGETS ${BIN_NAME} DESTINATION ${CMAKE_INSTALL_LIBEXECDIR})
