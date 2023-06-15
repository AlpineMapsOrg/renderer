cmake_minimum_required(VERSION 3.21)
project(alpine-renderer-app LANGUAGES CXX)

qt_add_executable(alpineapp
    main.cpp
    RenderThreadNotifier.h RenderThreadNotifier.cpp
    TerrainRendererItem.h TerrainRendererItem.cpp
    GnssInformation.h GnssInformation.cpp
    TerrainRenderer.h TerrainRenderer.cpp
    HotReloader.h HotReloader.cpp
)

qt_add_qml_module(alpineapp
    URI app
    VERSION 1.0
    RESOURCE_PREFIX /qt/qml
    QML_FILES
        main_loader.qml
        Main.qml
        About.qml
        Map.qml
        SearchBox.qml
        SearchResults.qml
        Settings.qml
        Coordinates.qml
        RoundMapButton.qml
    RESOURCES
        icons/camera_operation_centre.svg
        icons/compass.svg
        icons/current_location.svg
        icons/mascot.jpg
        icons/menu.svg
        icons/peak.svg
        icons/search.svg
)

qt_add_resources(alpineapp "height_data"
    PREFIX "/map"
    BASE ${alpine_height_data_SOURCE_DIR}
    FILES ${alpine_height_data_SOURCE_DIR}/height_data.atb
)

qt_add_translations(alpineapp TS_FILES
    i18n/de.ts
    i18n/en.ts
)

qt_add_resources(alpineapp "fonts"
    BASE ${alpineapp_fonts_SOURCE_DIR}/TTF/
    PREFIX "/fonts"
    FILES
    ${alpineapp_fonts_SOURCE_DIR}/TTF/SourceSans3-BlackIt.ttf
    ${alpineapp_fonts_SOURCE_DIR}/TTF/SourceSans3-BoldIt.ttf
    ${alpineapp_fonts_SOURCE_DIR}/TTF/SourceSans3-ExtraLightIt.ttf
    ${alpineapp_fonts_SOURCE_DIR}/TTF/SourceSans3-It.ttf
    ${alpineapp_fonts_SOURCE_DIR}/TTF/SourceSans3-Light.ttf
    ${alpineapp_fonts_SOURCE_DIR}/TTF/SourceSans3-Medium.ttf
    ${alpineapp_fonts_SOURCE_DIR}/TTF/SourceSans3-SemiboldIt.ttf
    ${alpineapp_fonts_SOURCE_DIR}/TTF/SourceSans3-Black.ttf
    ${alpineapp_fonts_SOURCE_DIR}/TTF/SourceSans3-Bold.ttf
    ${alpineapp_fonts_SOURCE_DIR}/TTF/SourceSans3-ExtraLight.ttf
    ${alpineapp_fonts_SOURCE_DIR}/TTF/SourceSans3-LightIt.ttf
    ${alpineapp_fonts_SOURCE_DIR}/TTF/SourceSans3-MediumIt.ttf
    ${alpineapp_fonts_SOURCE_DIR}/TTF/SourceSans3-Regular.ttf
    ${alpineapp_fonts_SOURCE_DIR}/TTF/SourceSans3-Semibold.ttf
)

set_target_properties(alpineapp PROPERTIES
    QT_ANDROID_PACKAGE_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/android
)
target_link_libraries(alpineapp PUBLIC nucleus gl_engine Qt::Quick Qt::QuickControls2)
if (ALP_ENABLE_POSITIONING)
    target_link_libraries(alpineapp PUBLIC Qt::Positioning)
    target_compile_definitions(alpineapp PUBLIC "ALP_ENABLE_GNSS")
endif()

if (ANDROID OR EMSCRIPTEN)
    target_compile_definitions(alpineapp PUBLIC ALP_QML_SOURCE_DIR="qrc:///qt/qml/app/")
else()
    target_compile_definitions(alpineapp PUBLIC ALP_QML_SOURCE_DIR="file:/${CMAKE_CURRENT_SOURCE_DIR}/")
endif()

if (ANDROID)
    add_android_openssl_libraries_to(alpineapp)

    install(TARGETS alpineapp
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

if (EMSCRIPTEN)
    set(ALP_INSTALL_FILES
        "$<TARGET_FILE_DIR:alpineapp>/alpineapp.js"
        "$<TARGET_FILE_DIR:alpineapp>/alpineapp.wasm"
        "$<TARGET_FILE_DIR:alpineapp>/alpineapp.html"
        "$<TARGET_FILE_DIR:alpineapp>/qtloader.js"
    )

if (ALP_ENABLE_THREADING)
    list(APPEND ALP_INSTALL_FILES "$<TARGET_FILE_DIR:alpineapp>/alpineapp.worker.js")
endif()
install(FILES ${ALP_INSTALL_FILES} DESTINATION ${ALP_WWW_INSTALL_DIR})
endif()
