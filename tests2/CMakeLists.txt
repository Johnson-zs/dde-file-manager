cmake_minimum_required(VERSION 3.10)

# 项目名称和版本
project(dde-file-manager-tests2 VERSION 1.0.0)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置Qt自动化工具
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# 编译选项
set(CMAKE_CXX_FLAGS "")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -Wall -Wl,--as-needed")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fstack-protector-all -Werror=return-type")

# 测试模式环境变量检查
if(NOT DEFINED ENV{DFM_TEST_MODE})
    message(WARNING "DFM_TEST_MODE environment variable not set. Please set DFM_TEST_MODE=1 for safe testing.")
endif()

# 添加必要的编译定义
add_definitions(-DQT_MESSAGELOGCONTEXT)
add_definitions(-DENABLE_TESTING)
add_definitions(-DDEBUG_STUB_INVOKE)

# 构建类型配置
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING
        "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel."
        FORCE)
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# Debug模式的特殊配置
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_VERBOSE_MAKEFILE ON)
    add_compile_definitions(ENABLE_TSAN_TOOL)
    message(STATUS "Debug mode: enabling sanitize checks")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=undefined,address,leak -fno-omit-frame-pointer")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=undefined,address,leak -fno-omit-frame-pointer")
    set(CMAKE_L_FLAGS "${CMAKE_L_FLAGS} -fsanitize=undefined,address,leak -fno-omit-frame-pointer")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Ofast")
endif()

# 覆盖率编译选项
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-inline -fno-access-control -O0 -fprofile-arcs -ftest-coverage")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DQT_DEBUG")

message(STATUS "CMAKE_CXX_FLAGS: ${CMAKE_CXX_FLAGS}")

# 检测是否作为子项目编译
get_directory_property(HAS_PARENT PARENT_DIRECTORY)
if(HAS_PARENT)
    message(STATUS "Building as subproject")
    set(IS_SUBPROJECT TRUE)
else()
    message(STATUS "Building as standalone project")
    set(IS_SUBPROJECT FALSE)
endif()

# 设置项目路径
if(IS_SUBPROJECT)
    # 作为子项目时，使用父项目的路径
    set(PROJECT_ROOT_PATH "${CMAKE_SOURCE_DIR}")
    set(PROJECT_INCLUDE_PATH "${CMAKE_SOURCE_DIR}/include")
    set(PROJECT_SOURCE_PATH "${CMAKE_SOURCE_DIR}/src")
else()
    # 独立编译时，需要找到主项目路径
    get_filename_component(PROJECT_ROOT_PATH "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
    set(PROJECT_INCLUDE_PATH "${PROJECT_ROOT_PATH}/include")
    set(PROJECT_SOURCE_PATH "${PROJECT_ROOT_PATH}/src")
endif()

message(STATUS "Project root path: ${PROJECT_ROOT_PATH}")
message(STATUS "Project include path: ${PROJECT_INCLUDE_PATH}")
message(STATUS "Project source path: ${PROJECT_SOURCE_PATH}")

# 包含路径
include_directories(${PROJECT_SOURCE_PATH})
include_directories(${PROJECT_INCLUDE_PATH})

# 查找依赖包
find_package(Qt6 COMPONENTS 
    Core 
    Concurrent
    Test
    REQUIRED
)

find_package(Dtk6 COMPONENTS Core REQUIRED)
find_package(GTest REQUIRED)
find_package(PkgConfig REQUIRED)

# GTest配置
include_directories(${GTEST_INCLUDE_DIRS})

# 打桩工具配置
set(TEST_UTILS_PATH "${PROJECT_ROOT_PATH}/3rdparty/testutils")
if(EXISTS "${TEST_UTILS_PATH}")
    file(GLOB CPP_STUB_SRC 
        "${TEST_UTILS_PATH}/cpp-stub/*.h"
        "${TEST_UTILS_PATH}/cpp-stub/*.hpp"
        "${TEST_UTILS_PATH}/stub-ext/*.h"
        "${TEST_UTILS_PATH}/stub-ext/*.cpp"
    )
    include_directories("${TEST_UTILS_PATH}/cpp-stub")
    include_directories("${TEST_UTILS_PATH}/stub-ext")
    message(STATUS "Found cpp-stub at: ${TEST_UTILS_PATH}")
else()
    message(WARNING "cpp-stub not found at: ${TEST_UTILS_PATH}")
endif()

# 如果不是子项目，需要构建dfm-framework
if(NOT IS_SUBPROJECT)
    message(STATUS "Building dfm-framework as external dependency")
    
    # 添加dfm-framework作为外部项目
    add_subdirectory(${PROJECT_SOURCE_PATH}/dfm-framework dfm-framework)
    
    # 确保dfm-framework目标可用
    if(NOT TARGET DFM6::framework)
        message(FATAL_ERROR "DFM6::framework target not found")
    endif()
endif()

# 通用测试工具
add_subdirectory(common)

# 测试模块
add_subdirectory(dfm-framework)

# 启用测试
enable_testing()

message(STATUS "Tests2 configuration completed successfully") 