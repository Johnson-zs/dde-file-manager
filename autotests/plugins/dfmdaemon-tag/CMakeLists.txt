cmake_minimum_required(VERSION 3.10)

# Use DFM enhanced test utilities to create plugin test
set(DFM_TEST_NO_DEBUG ON)
dfm_create_plugin_test_enhanced("dfmdaemon-tag" "${DFM_SOURCE_DIR}/plugins/daemon/tag") 
