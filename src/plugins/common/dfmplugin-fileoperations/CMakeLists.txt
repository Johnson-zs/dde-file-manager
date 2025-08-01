cmake_minimum_required(VERSION 3.10)

project(dfmplugin-fileoperations)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

# Include shared dependencies configuration
include(${CMAKE_CURRENT_SOURCE_DIR}/dependencies.cmake)

# 设置二进制文件名
set(BIN_NAME dfm-fileoperations-plugin)

FILE(GLOB_RECURSE FILEOPERATIONS_FILES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.json"
)

# 创建共享库
add_library(${BIN_NAME}
    SHARED
    ${FILEOPERATIONS_FILES}
)

set_target_properties(${BIN_NAME} PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${DFM_BUILD_PLUGIN_COMMON_DIR})

# Apply fileoperations plugin specific configuration
dfm_setup_fileoperations_dependencies(${BIN_NAME})

# 安装库文件
install(TARGETS
    ${BIN_NAME}
    LIBRARY
    DESTINATION
    ${DFM_PLUGIN_COMMON_CORE_DIR}
)

# 安装 dconfig 文件
INSTALL_DCONFIG("org.deepin.dde.file-manager.operations.json")
