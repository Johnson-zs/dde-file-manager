cmake_minimum_required(VERSION 3.10)

add_subdirectory(dfm-base)
add_subdirectory(dfm-extension)
add_subdirectory(dfm-framework)
add_subdirectory(dde-desktop-widgets)

if(OPT_ENABLE_QML)
    add_subdirectory(dfm-gui)
    add_subdirectory(dfm-declarative)
endif()
