cmake_minimum_required(VERSION 3.10)
set(CMAKE_AUTOMOC OFF)
set(CMAKE_AUTORCC OFF)
set(CMAKE_AUTOUIC OFF)
project(redbox)

FILE(GLOB_RECURSE SRCS "${CMAKE_CURRENT_SOURCE_DIR}/src/*")

include_directories(${CMAKE_SOURCE_DIR}/src)

add_library(${PROJECT_NAME}
    SHARED
    ${SRCS}
)

set_target_properties(${PROJECT_NAME} PROPERTIES LIBRARY_OUTPUT_DIRECTORY ../../apps)

target_link_libraries(${PROJECT_NAME} PRIVATE pthread)

