# dependencies.cmake - Dependencies configuration for dfmplugin-filegroup
# This file defines the specific dependencies and configuration for the filegroup plugin

cmake_minimum_required(VERSION 3.10)

# Include DFM plugin configuration module
include(DFMPluginConfig)

# Function to setup filegroup plugin dependencies
function(dfm_setup_filegroup_dependencies target_name)
    message(STATUS "DFM: Setting up filegroup plugin dependencies for: ${target_name}")
    
    # Find required packages
    find_package(Qt6 REQUIRED COMPONENTS Core DBus Widgets)
    
    # Apply default plugin configuration first
    dfm_apply_default_plugin_config(${target_name})
    
    # Add filegroup-specific dependencies
    target_link_libraries(${target_name} PRIVATE
        Qt6::Core
        Qt6::Widgets
        Qt6::DBus
    )
    
    message(STATUS "DFM: Filegroup plugin dependencies configured successfully")
endfunction()

message(STATUS "DFM: Filegroup plugin dependencies configuration loaded") 