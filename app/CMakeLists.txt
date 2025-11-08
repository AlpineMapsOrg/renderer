#############################################################################
# Alpine Terrain Renderer
# Copyright (C) 2023 Adam Celarek <family name at cg tuwien ac at>
# Copyright (C) 2023 Gerald Kimmersdorfer
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#############################################################################

project(alpine-renderer-app LANGUAGES CXX)

option(ALP_ENABLE_QML_HOT_RELOAD "Enables hot-reloading of qml files (which slows down compilation)" OFF)

qt_add_executable(alpineapp
    main.cpp
    RenderThreadNotifier.h RenderThreadNotifier.cpp
    TerrainRenderer.h TerrainRenderer.cpp
    AppSettings.h AppSettings.cpp
    TerrainRendererItem.h TerrainRendererItem.cpp
    GnssInformation.h GnssInformation.cpp
    TrackModel.h TrackModel.cpp
    ModelBinding.h ModelBinding.cpp
    RenderingContext.h RenderingContext.cpp
)

qt_add_qml_module(alpineapp
    URI app
    VERSION 1.0
    RESOURCE_PREFIX /qt/qml
    RESOURCES
        icons/favicon.ico
        icons/mascot.jpg
        icons/menu.png
        icons/search.png
        icons/icon.png
        icons/material/monitoring.png
        icons/material/3d_rotation.png
        icons/material/map.png
        icons/material/pin_drop.png
        icons/material/settings.png
        icons/material/info.png
        icons/material/format_paint.png
        icons/material/location_searching.png
        icons/material/my_location.png
        icons/material/navigation.png
        icons/material/navigation_offset.png
        icons/material/add.png
        icons/material/chevron_left.png
        icons/material/visibility_off.png
        icons/material/filter_alt.png
        icons/presets/basic.png
        icons/presets/shaded.png
        icons/presets/snow.png
        icons/needle_head_down.png
        icons/logo_type_horizontal.png
        icons/logo_type_vertical.png
        icons/logo_type_horizontal_short.png
    QML_FILES
        Main.qml
        About.qml
        Map.qml
        SearchBox.qml
        SearchResults.qml
        Settings.qml
        Coordinates.qml
        GeneralSettings.qml
        GlSettings.qml
        FloatingActionButtonGroup.qml
        FilterWindow.qml
        PickerWindow.qml
        components/LabledSlider.qml
        components/LabledRangeSlider.qml
        components/ColorPicker.qml
        components/PageDrawer.qml
        components/CheckGroup.qml
        components/SettingsPanel.qml
        components/SettingsTitle.qml
        components/DrawerButton.qml
        components/DrawerSeparator.qml
        components/DrawerSpacer.qml
        components/VectorEditor.qml
        components/DatePicker.qml
        components/DateMonthTablePicker.qml
        components/FloatingActionButton.qml
        components/FluxColor/AngleWheel.qml
        components/FluxColor/ColorChooser.qml
        components/FluxColor/HueRing.qml
        components/FluxColor/HueWheel.qml
        components/FluxColor/WheelArea.qml
        picker/PoiPeak.qml
        picker/PoiWebcam.qml
        picker/Default.qml
        picker/PoiAlpineHut.qml
        picker/PoiSettlement.qml
        SOURCES TileStatistics.h TileStatistics.cpp
)

qt_add_resources(alpineapp "fonts"
    BASE ${alpineapp_fonts_SOURCE_DIR}/
    PREFIX "/fonts"
    FILES
    ${alpineapp_fonts_SOURCE_DIR}/Roboto/Roboto-Regular.ttf
)

qt_add_translations(alpineapp TS_FILES
    i18n/de.ts
    i18n/en.ts
)

alp_get_version(
    GIT_DIR             "${CMAKE_CURRENT_SOURCE_DIR}"
    VERSION_VAR         "ALP_VERSION"
    VERSION_INT_VAR     "ALP_VERSION_INTEGER"
)

set_target_properties(alpineapp PROPERTIES
    QT_ANDROID_PACKAGE_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/android
    QT_ANDROID_VERSION_NAME ${ALP_VERSION}
    QT_ANDROID_VERSION_CODE "${ALP_VERSION_INTEGER}"
)
target_link_libraries(alpineapp PUBLIC gl_engine Qt::Quick Qt::QuickControls2)

if (ALP_ENABLE_QML_HOT_RELOAD)
    message(WARNING "building alpine app with qml-hot-reload. It'll slow incremental building due to a fix for hotreload.")

    # change prefer to project dir, as per https://bugreports.qt.io/browse/QTBUG-120435
    # otherwise we can't hotreload files in app when there is "import app" in the qml file.
    # but we need import app, if we want to use the new approach of registering qml types (directly in the class, not in main).
    add_custom_command(
        TARGET alpineapp POST_BUILD
        COMMAND ${CMAKE_COMMAND}
          -DBUILD_DIR="${CMAKE_CURRENT_BINARY_DIR}/app"
          -DSOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
          -P "${CMAKE_SOURCE_DIR}/cmake/alp_fix_qmldir_for_hotreload.cmake"
    )
endif()

if (ALP_ENABLE_DEV_TOOLS)
    message(WARNING "building alpine app with dev tools. this will increase the build size.")
    qt_target_qml_sources(alpineapp
        QML_FILES
            StatsWindow.qml
            loader_dev.qml
        )
    target_sources(alpineapp
        PUBLIC
            HotReloader.h HotReloader.cpp
            TimerFrontendManager.h TimerFrontendManager.cpp
            TimerFrontendObject.h TimerFrontendObject.cpp
    )
    target_link_libraries(alpineapp PUBLIC Qt::Charts Qt::Widgets)
else()
    qt_target_qml_sources(alpineapp QML_FILES loader.qml)
endif()

if (ALP_ENABLE_POSITIONING)
    target_link_libraries(alpineapp PUBLIC Qt::Positioning)
    target_compile_definitions(alpineapp PUBLIC "ALP_ENABLE_GNSS")
endif()
if (ALP_ENABLE_APP_SHUTDOWN_AFTER_60S)
    target_compile_definitions(alpineapp PUBLIC "ALP_APP_SHUTDOWN_AFTER_60S")
endif()

if (ANDROID OR EMSCRIPTEN)
    target_compile_definitions(alpineapp PUBLIC ALP_QML_SOURCE_DIR="qrc:///qt/qml/app/")
else()
    target_compile_definitions(alpineapp PUBLIC ALP_QML_SOURCE_DIR="file:/${CMAKE_CURRENT_SOURCE_DIR}/")
endif()

if (ANDROID)
    add_android_openssl_libraries(alpineapp)
    find_package(Qt6 REQUIRED COMPONENTS Widgets) # for QFileDialogue (adding tracks)
    target_link_libraries(alpineapp PUBLIC Qt::Widgets)

    install(TARGETS alpineapp
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

if (EMSCRIPTEN)
    target_link_options(alpineapp PUBLIC -sASYNCIFY)

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
else()
    find_package(Qt6 REQUIRED COMPONENTS Widgets)
    target_link_libraries(alpineapp PUBLIC Qt::Widgets)
endif()
