cmake_minimum_required(VERSION 3.14)
project(ZkEligibilityUiPlugin LANGUAGES CXX)

if(DEFINED ENV{LOGOS_MODULE_BUILDER_ROOT})
    include($ENV{LOGOS_MODULE_BUILDER_ROOT}/cmake/LogosModule.cmake)
else()
    message(FATAL_ERROR "LogosModule.cmake not found. Set LOGOS_MODULE_BUILDER_ROOT.")
endif()

logos_module(
    NAME zk_eligibility_ui
    REP_FILE src/ZkVerifyBackend.rep
    SOURCES
        src/ZkPluginInterface.h
        src/ZkPlugin.h
        src/ZkPlugin.cpp
        src/ZkVerifyBackend.h
        src/ZkVerifyBackend.cpp
    FIND_PACKAGES
        Qt6Gui
    LINK_LIBRARIES
        Qt6::Gui
)
