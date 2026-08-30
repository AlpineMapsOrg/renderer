#############################################################################
# AlpineMaps.org
# Copyright (C) 2026 Adam Celarek
# SPDX-License-Identifier: GPL-3.0-or-later
#############################################################################

project(alpine-renderer-texture-compression-benchmark LANGUAGES CXX)

qt_add_executable(texture_compression_benchmark
    main.cpp
    ${CMAKE_SOURCE_DIR}/apps/texture_compression_benchmark/TextureCompressionData.h
)

target_include_directories(texture_compression_benchmark PRIVATE
    ${CMAKE_SOURCE_DIR}/apps/texture_compression_benchmark
)
target_link_libraries(texture_compression_benchmark PUBLIC gl_engine Qt::Network Qt::OpenGL)
alp_configure_target(texture_compression_benchmark)

if (ANDROID)
    add_android_openssl_libraries(texture_compression_benchmark)
endif()

if (EMSCRIPTEN)
    install(
        FILES
            "$<TARGET_FILE_DIR:texture_compression_benchmark>/texture_compression_benchmark.js"
            "$<TARGET_FILE_DIR:texture_compression_benchmark>/texture_compression_benchmark.wasm"
            "$<TARGET_FILE_DIR:texture_compression_benchmark>/texture_compression_benchmark.html"
            "$<TARGET_FILE_DIR:texture_compression_benchmark>/qtloader.js"
        DESTINATION "${ALP_WWW_INSTALL_DIR}/texture_compression_benchmark"
    )
    install(
        FILES "$<TARGET_FILE_DIR:texture_compression_benchmark>/texture_compression_benchmark.worker.js"
        DESTINATION "${ALP_WWW_INSTALL_DIR}/texture_compression_benchmark"
        OPTIONAL
    )
endif()
