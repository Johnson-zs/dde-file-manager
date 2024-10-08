cmake_minimum_required(VERSION 3.10)

set(VERSION "1.0.0" CACHE STRING "define project version")
set(BIN_NAME dde-desktop-widgets)
# find source code
file(GLOB_RECURSE INCLUDE_FILES  CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/include/dde-desktop-widgets/*")
file(GLOB_RECURSE SRCS CONFIGURE_DEPENDS
    "./*.h"
    "./*.cpp"
)

find_package(Qt6 COMPONENTS Core Gui Widgets REQUIRED)
find_package(Dtk6 REQUIRED COMPONENTS Core Gui Widget)

add_library(${BIN_NAME} SHARED
    ${INCLUDE_FILES}
    ${SRCS}
)

target_link_libraries(${BIN_NAME}
PUBLIC
    Dtk6::Core
    Dtk6::Gui
    Dtk6::Widget
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

target_include_directories(${BIN_NAME} PUBLIC
    ${PROJECT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/libs
)

if (NOT VERSION)
    set(VERSION "1.0.0")
endif()

if (NOT PROJECT_VERSION_MAJOR)
    set(PROJECT_VERSION_MAJOR 1)
endif()

set_target_properties(${BIN_NAME} PROPERTIES
    VERSION ${VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

target_compile_definitions(${BIN_NAME} PRIVATE VERSION="${CMAKE_PROJECT_VERSION}")

# install dev
install(TARGETS ${BIN_NAME} LIBRARY DESTINATION ${LIB_INSTALL_DIR})
install(DIRECTORY
    ${PROJECT_SOURCE_DIR}/include/${BIN_NAME}
    DESTINATION include
    FILES_MATCHING PATTERN "*.h"
)

# config cmake file
set(PLUGIN_INSTALL_DIR ${CMAKE_INSTALL_FULL_LIBDIR}/${BIN_NAME}/plugins)
include(CMakePackageConfigHelpers)
configure_package_config_file(
    ${PROJECT_SOURCE_DIR}/assets/dev/${BIN_NAME}/DDEDesktopWidgetsConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/DDEDesktopWidgetsConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/DDEDesktopWidgets
    PATH_VARS PLUGIN_INSTALL_DIR
    INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX})
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/DDEDesktopWidgetsConfig.cmake DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/DDEDesktopWidgets)

