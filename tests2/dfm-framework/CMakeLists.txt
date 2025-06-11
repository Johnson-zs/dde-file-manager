# dfm-framework 测试模块

# 测试可执行文件名
set(TEST_TARGET_NAME test-dfm-framework)

# 查找所有测试源文件
file(GLOB_RECURSE TEST_SOURCES
    "*.cpp"
    "*.h"
)

# 排除CMakeLists.txt
list(FILTER TEST_SOURCES EXCLUDE REGEX "CMakeLists.txt")

# 如果没有测试文件，创建一个基础的测试文件
if(NOT TEST_SOURCES)
    set(TEST_SOURCES
        basic_test.cpp
    )
endif()

# 创建测试可执行文件
add_executable(${TEST_TARGET_NAME} ${TEST_SOURCES})

# 链接依赖库
target_link_libraries(${TEST_TARGET_NAME} PRIVATE
    Tests2::Common
    DFM6::framework
    Qt6::Core
    Qt6::Concurrent
    Qt6::Test
    Dtk6::Core
    ${GTEST_LIBRARIES}
    pthread
    stdc++fs
    gcov
)

# 包含cpp-stub源文件
if(CPP_STUB_SRC)
    target_sources(${TEST_TARGET_NAME} PRIVATE ${CPP_STUB_SRC})
endif()

# 设置包含目录
target_include_directories(${TEST_TARGET_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_INCLUDE_PATH}
    ${PROJECT_SOURCE_PATH}
    ${PROJECT_SOURCE_PATH}/dfm-framework
)

# 编译定义
target_compile_definitions(${TEST_TARGET_NAME} PRIVATE
    QT_MESSAGELOGCONTEXT
    ENABLE_TESTING
    DEBUG_STUB_INVOKE
    DPF_INTERNAL_USE=1
)

# 添加到测试套件
add_test(NAME ${TEST_TARGET_NAME} COMMAND ${TEST_TARGET_NAME})

# 设置测试环境变量
set_tests_properties(${TEST_TARGET_NAME} PROPERTIES
    ENVIRONMENT "DFM_TEST_MODE=1"
    TIMEOUT 300
)

message(STATUS "dfm-framework test module configured")
message(STATUS "Test sources: ${TEST_SOURCES}") 