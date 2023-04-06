cmake_minimum_required(VERSION 3.1)
project(webrtc_rustysignal VERSION 0.0)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/.orogen/config")
include(webrtc_rustysignalBase)

if (ROCK_TEST_ENABLED)
    enable_testing()
    find_package(Syskit REQUIRED)
    syskit_orogen_tests(test)
endif()

# FIND_PACKAGE(KDL)
# FIND_PACKAGE(OCL)

