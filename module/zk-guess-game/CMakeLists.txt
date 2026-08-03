cmake_minimum_required(VERSION 3.14)
project(ZkGuessGamePlugin LANGUAGES CXX)

if(DEFINED ENV{LOGOS_MODULE_BUILDER_ROOT})
    include($ENV{LOGOS_MODULE_BUILDER_ROOT}/cmake/LogosModule.cmake)
else()
    message(FATAL_ERROR "LogosModule.cmake not found. Set LOGOS_MODULE_BUILDER_ROOT.")
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(sodium REQUIRED IMPORTED_TARGET libsodium)   # StationCrypto: Argon2id KDF + XChaCha20 AEAD

logos_module(
    NAME zk_guess_game
    REP_FILE src/zk_guess_game.rep
    SOURCES
        src/zk_guess_game_backend.h
        src/zk_guess_game_backend.cpp
        src/station_crypto.h
        src/station_crypto.cpp
    INCLUDE_DIRS
        src
    LINK_LIBRARIES
        PkgConfig::sodium
)
