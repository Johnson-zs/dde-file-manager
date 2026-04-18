cmake_minimum_required(VERSION 3.10)

project(test-luceneindex-demo)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find packages
find_package(PkgConfig REQUIRED)
find_package(Qt6 COMPONENTS Core Widgets Concurrent REQUIRED)
find_package(Dtk6 COMPONENTS Core Widget REQUIRED)

# Find lucene++ and docparser
pkg_check_modules(Lucene REQUIRED IMPORTED_TARGET liblucene++ liblucene++-contrib)
pkg_check_modules(Docparser REQUIRED IMPORTED_TARGET docparser)
pkg_check_modules(GLIB REQUIRED glib-2.0)

# Collect source files
FILE(GLOB_RECURSE LUCENEINDEX_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

# Add fulltext sources (ChineseAnalyzer)
set(FULL_TEXT_PATH "${CMAKE_SOURCE_DIR}/3rdparty/fulltext")
file(GLOB FULLTEXT_SOURCES
    "${FULL_TEXT_PATH}/*.cpp"
    "${FULL_TEXT_PATH}/*.h"
)

add_executable(${PROJECT_NAME}
    ${LUCENEINDEX_FILES}
    ${FULLTEXT_SOURCES}
)

add_executable(dfm-luceneindex-demo ALIAS ${PROJECT_NAME})

set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    dfm6-base
    Qt6::Core
    Qt6::Widgets
    Qt6::Concurrent
    Dtk6::Core
    Dtk6::Widget
    ${GLIB_LIBRARIES}
    PkgConfig::Lucene
    PkgConfig::Docparser
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src/dfm-base
    ${CMAKE_SOURCE_DIR}/src/dfm-base/base
    ${CMAKE_SOURCE_DIR}/src/dfm-base/utils
    ${CMAKE_SOURCE_DIR}/src/dfm-base/file
    ${CMAKE_SOURCE_DIR}/src/dfm-base/interfaces
    ${CMAKE_SOURCE_DIR}/3rdparty
    ${GLIB_INCLUDE_DIRS}
)
