# 通用测试工具库

set(COMMON_LIB_NAME test-common)

# 通用测试工具源文件
set(COMMON_SOURCES
    testutils.cpp
    testutils.h
)

# 创建通用测试工具库
add_library(${COMMON_LIB_NAME} STATIC ${COMMON_SOURCES})

# 链接依赖
target_link_libraries(${COMMON_LIB_NAME} PUBLIC
    Qt6::Core
    Qt6::Test
    ${GTEST_LIBRARIES}
    pthread
    stdc++fs
    gcov
)

# 包含目录
target_include_directories(${COMMON_LIB_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_INCLUDE_PATH}
    ${PROJECT_SOURCE_PATH}
)

# 编译定义
target_compile_definitions(${COMMON_LIB_NAME} PUBLIC
    QT_MESSAGELOGCONTEXT
    ENABLE_TESTING
    DEBUG_STUB_INVOKE
)

# 创建别名
add_library(Tests2::Common ALIAS ${COMMON_LIB_NAME})

message(STATUS "Common test utilities configured") 