cmake_minimum_required(VERSION 3.10)

# If do't define version number, specify the version number
set (VERSION "1.0.0" CACHE STRING "define project version")

set(BIN_NAME dfm6-framework)
set(OLD_BIN_NAME dfm-framework)

# find source code
file(GLOB_RECURSE INCLUDE_FILES  CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/include/dfm-framework/*")
file(GLOB_RECURSE SRCS CONFIGURE_DEPENDS
    "./*.h"
    "./*.cpp"
)

add_library(${BIN_NAME} SHARED
    ${INCLUDE_FILES}
    ${SRCS}
)

# Configure library using unified configuration function
dfm_configure_framework_library(${BIN_NAME})

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

# 创建 ALIAS 目标，这样其他组件可以使用 DFM6::framework 来引用这个库
add_library(DFM6::framework ALIAS ${BIN_NAME})

# install headers
install(DIRECTORY
    ${PROJECT_SOURCE_DIR}/include/${OLD_BIN_NAME}
    DESTINATION include
    FILES_MATCHING PATTERN "*.h"
)

# install lib
install(TARGETS ${BIN_NAME} LIBRARY DESTINATION ${LIB_INSTALL_DIR})

# install dev
# for pc file config
set(PC_LIBS_PRIVATE Qt6Core)
set(PC_REQ_PRIVATE)
set(PC_REQ_PUBLIC)

# config pkgconfig file
configure_file(${PROJECT_SOURCE_DIR}/assets/dev/${BIN_NAME}/${BIN_NAME}.pc.in ${BIN_NAME}.pc @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${BIN_NAME}.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)

# config cmake file
configure_file(${PROJECT_SOURCE_DIR}/assets/dev/${BIN_NAME}/${BIN_NAME}Config.cmake.in ${BIN_NAME}Config.cmake @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${BIN_NAME}Config.cmake DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${BIN_NAME})




