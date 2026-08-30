#############################################################################
# AlpineMaps.org
# Copyright (C) 2026 Adam Celarek
# SPDX-License-Identifier: GPL-3.0-or-later
#############################################################################

project(texture-compression-preview LANGUAGES CXX)

qt_add_executable(texture_compression_preview
    main.cpp
    TextureCompressionData.h
    TexturePreviewItem.h
    TexturePreviewItem.cpp
)

qt_add_qml_module(texture_compression_preview
    URI TextureCompressionPreview
    VERSION 1.0
    RESOURCE_PREFIX /qt/qml
    QML_FILES Main.qml
)

qt_add_resources(texture_compression_preview "fonts"
    BASE "${alpineapp_fonts_SOURCE_DIR}"
    PREFIX "/fonts"
    FILES "${alpineapp_fonts_SOURCE_DIR}/Roboto/Roboto-Regular.ttf"
)

target_link_libraries(texture_compression_preview PUBLIC gl_engine Qt::Network Qt::Quick Qt::QuickControls2)
alp_configure_target(texture_compression_preview)

if (ANDROID)
    add_android_openssl_libraries(texture_compression_preview)
    set_target_properties(texture_compression_preview PROPERTIES
        QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android"
        QT_ANDROID_PACKAGE_NAME "org.alpinemaps.texturecompressionpreview"
        QT_ANDROID_VERSION_NAME "1.0"
        QT_ANDROID_VERSION_CODE 1
    )
    install(TARGETS texture_compression_preview
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

if (EMSCRIPTEN)
    install(
        FILES
            "$<TARGET_FILE_DIR:texture_compression_preview>/texture_compression_preview.js"
            "$<TARGET_FILE_DIR:texture_compression_preview>/texture_compression_preview.wasm"
            "$<TARGET_FILE_DIR:texture_compression_preview>/texture_compression_preview.html"
            "$<TARGET_FILE_DIR:texture_compression_preview>/qtloader.js"
        DESTINATION "${ALP_WWW_INSTALL_DIR}/texture_compression_preview"
    )
    install(
        FILES "$<TARGET_FILE_DIR:texture_compression_preview>/texture_compression_preview.worker.js"
        DESTINATION "${ALP_WWW_INSTALL_DIR}/texture_compression_preview"
        OPTIONAL
    )
endif()
