cmake_minimum_required(VERSION 3.10)

project(dfmplugin-sidebar)
set(BIN_NAME dfm-sidebar-plugin)

add_definitions(-DSIDEBAR_TREEVIEW)
set(SIDEBAR_TREEVIEW ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

if (SIDEBAR_TREEVIEW)
    add_definitions(-DTREEVIEW)
    message("Sidebar is implemented with tree view")
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/treeviews)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/treemodels)
    FILE(GLOB SIDEBAR_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/utils/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/utils/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/events/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/events/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/treemodels/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/treemodels/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/treeviews/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/treeviews/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/treeviews/private/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.json"
    )
else()
    message("Sidebar is implemented with list view")
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/views)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/models)
FILE(GLOB SIDEBAR_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/utils/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/utils/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/events/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/events/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/models/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/models/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/views/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/views/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/views/private/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.json"
    )
endif()

find_package(Qt6 COMPONENTS Core)
add_library(${BIN_NAME}
    SHARED
    ${SIDEBAR_FILES}
)

set_target_properties(${BIN_NAME} PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${DFM_BUILD_PLUGIN_FILEMANAGER_DIR})

target_link_libraries(${BIN_NAME}
    DFM6::base
    DFM6::framework
)

#install library file
install(TARGETS
    ${BIN_NAME}
    LIBRARY
    DESTINATION
    ${DFM_PLUGIN_FILEMANAGER_CORE_DIR}
)

# install dconfig files.
INSTALL_DCONFIG("org.deepin.dde.file-manager.sidebar.json")

