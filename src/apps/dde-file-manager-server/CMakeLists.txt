cmake_minimum_required(VERSION 3.1.0)

project(dde-file-manager-server)

configure_file(
    "config.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/config.h"
)

set(SERVER_SRCS
    main.cpp
)

find_package(Qt5 COMPONENTS Widgets REQUIRED)
find_package(Dtk REQUIRED Widget REQUIRED)

add_executable(${PROJECT_NAME} ${SERVER_SRCS})

target_include_directories(${PROJECT_NAME} 
    PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}
        ${DtkWidget_INCLUDE_DIRS}
)

target_link_libraries(
    ${PROJECT_NAME}
    Qt5::Widgets
    DFM::framework
    ${DtkWidget_LIBRARIES}
)

install(TARGETS ${PROJECT_NAME} DESTINATION bin)
