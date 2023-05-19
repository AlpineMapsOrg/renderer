cmake_minimum_required(VERSION 3.21)
project(alpine-renderer LANGUAGES CXX)

option(ALP_UNITTESTS "include unit test targets in the buildsystem" ON)
option(ALP_ENABLE_ADDRESS_SANITIZER "compiles atb with address sanitizer enabled (only debug, works only on g++ and clang)" OFF)
option(ALP_ENABLE_THREAD_SANITIZER "compiles atb with thread sanitizer enabled (only debug, works only on g++ and clang)" OFF)
option(ALP_ENABLE_ASSERTS "enable asserts (do not define NDEBUG)" ON)

if (EMSCRIPTEN)
    set(ALP_WWW_INSTALL_DIR "${CMAKE_CURRENT_BINARY_DIR}" CACHE PATH "path to the install directory (for webassembly files, i.e., www directory)")
    option(ALP_ENABLE_THREADING "Puts the scheduler into an extra thread." OFF)
else()
    option(ALP_ENABLE_THREADING "Puts the scheduler into an extra thread." ON)
endif()

if (NOT EMSCRIPTEN)
    option(ALP_ENABLE_POSITIONING "enable qt positioning (gnss / gps)" ON)
endif()

if (UNIX AND NOT EMSCRIPTEN AND NOT ANDROID)
    option(ALP_USE_LLVM_LINKER "use lld (llvm) for linking. it's parallel and much faster, but not installed by default.
        if it's not installed, you'll get errors, that openmp or other stuff is not installed (hard to track down)" OFF)
endif()


############################################ sources ##############################################
set(ALP_NUCLEUS_SOURCES
    nucleus/AbstractRenderWindow.h nucleus/AbstractRenderWindow.cpp
    nucleus/Controller.h nucleus/Controller.cpp
    nucleus/event_parameter.h
    nucleus/Raster.h
    nucleus/srs.h nucleus/srs.cpp
    nucleus/Tile.cpp nucleus/Tile.h
    nucleus/tile_scheduler/utils.h nucleus/tile_scheduler/utils.cpp
    nucleus/tile_scheduler/DrawListGenerator.h nucleus/tile_scheduler/DrawListGenerator.cpp
    nucleus/tile_scheduler/LayerAssembler.h nucleus/tile_scheduler/LayerAssembler.cpp
    nucleus/tile_scheduler/tile_types.h
    nucleus/tile_scheduler/constants.h
    nucleus/tile_scheduler/QuadAssembler.h nucleus/tile_scheduler/QuadAssembler.cpp
    nucleus/tile_scheduler/Cache.h
    nucleus/tile_scheduler/TileLoadService.h nucleus/tile_scheduler/TileLoadService.cpp
    nucleus/tile_scheduler/Scheduler.h nucleus/tile_scheduler/Scheduler.cpp
    nucleus/tile_scheduler/SlotLimiter.h nucleus/tile_scheduler/SlotLimiter.cpp
    nucleus/tile_scheduler/RateLimiter.h nucleus/tile_scheduler/RateLimiter.cpp
    nucleus/camera/CadInteraction.h nucleus/camera/CadInteraction.cpp
    nucleus/camera/Controller.h nucleus/camera/Controller.cpp
    nucleus/camera/Definition.h nucleus/camera/Definition.cpp
    nucleus/camera/FirstPersonInteraction.h nucleus/camera/FirstPersonInteraction.cpp
    nucleus/camera/FirstPersonInteraction2.h nucleus/camera/FirstPersonInteraction2.cpp
    nucleus/camera/InteractionStyle.h nucleus/camera/InteractionStyle.cpp
    nucleus/camera/OrbitInteraction.h nucleus/camera/OrbitInteraction.cpp
    nucleus/camera/RotateNorthInteraction.h nucleus/camera/RotateNorthInteraction.cpp
    nucleus/camera/AbstractDepthTester.h
    nucleus/camera/stored_positions.h
    nucleus/utils/Stopwatch.h nucleus/utils/Stopwatch.cpp
    nucleus/utils/terrain_mesh_index_generator.h
    nucleus/utils/tile_conversion.h nucleus/utils/tile_conversion.cpp
    nucleus/map_label/MapLabelModel.h nucleus/map_label/MapLabelModel.cpp
    nucleus/map_label/CameraTransformationProxyModel.h nucleus/map_label/CameraTransformationProxyModel.cpp
    nucleus/map_label/MapLabel.h
    nucleus/map_label/AbstractMapLabelModel.h
    nucleus/map_label/CameraFilterProxyModel.h nucleus/map_label/CameraFilterProxyModel.cpp
    nucleus/utils/bit_coding.h
)
set(ALP_GL_SOURCES
    gl_engine/Atmosphere.h gl_engine/Atmosphere.cpp
    gl_engine/Framebuffer.h gl_engine/Framebuffer.cpp
    gl_engine/ShaderManager.h gl_engine/ShaderManager.cpp
    gl_engine/TileManager.h gl_engine/TileManager.cpp
    gl_engine/TileSet.h
    gl_engine/Window.cpp gl_engine/Window.h
    gl_engine/DebugPainter.h gl_engine/DebugPainter.cpp
    gl_engine/helpers.h
    gl_engine/ShaderProgram.h gl_engine/ShaderProgram.cpp
)
set(ALP_GL_SHADERS
    gl_shaders/atmosphere_bg.frag
    gl_shaders/atmosphere_implementation.frag
    gl_shaders/screen_copy.frag
    gl_shaders/screen_pass.vert
    gl_shaders/tile.frag
    gl_shaders/tile.vert
    gl_shaders/depth.frag
    gl_shaders/encoder.glsl
)
set(ALP_PLAIN_RENDERER_SOURCES
    plain_renderer/main.cpp
    plain_renderer/Window.h plain_renderer/Window.cpp
)
set(ALP_APP_SOURCES
    app/main.cpp
    app/RenderThreadNotifier.h app/RenderThreadNotifier.cpp
    app/TerrainRendererItem.h app/TerrainRendererItem.cpp
    app/GnssInformation.h app/GnssInformation.cpp
    app/TerrainRenderer.h app/TerrainRenderer.cpp
)
set(ALP_APP_QML
    app/main.qml
    app/About.qml
    app/Map.qml
    app/SearchBox.qml
    app/SearchResults.qml
    app/Settings.qml
    app/Coordinates.qml
)
set(ALP_APP_ICONS
    app/icons/camera_operation_centre.svg
    app/icons/compass.svg
    app/icons/current_location.svg
    app/icons/mascot.jpg
    app/icons/menu.svg
    app/icons/peak.svg
    app/icons/search.svg
)
set(ALP_CATCH_UNITTEST_GL_SOURCES
    gl_engine/Framebuffer.h gl_engine/Framebuffer.cpp
    gl_engine/ShaderProgram.h gl_engine/ShaderProgram.cpp
    unittests_gl/UnittestGlWindow.h unittests_gl/UnittestGlWindow.cpp
    unittests_gl/main.cpp
    unittests_gl/framebuffer.cpp
)
set(ALP_CATCH_UNITTEST_DATA
    unittests/data/170px-Jeune_bouquetin_de_face.jpg
    unittests/data/test-tile_ortho.jpeg
    unittests/data/test-tile.png
)
set(ALP_CATCH_UNITTEST_SOURCES
    unittests/main.cpp
    unittests/catch2_helpers.h
    unittests/test_Camera.cpp
    unittests/nucleus_utils_stopwatch.cpp
    unittests/test_DrawListGenerator.cpp
    unittests/test_helpers.h
    unittests/test_raster.cpp
    unittests/test_terrain_mesh_index_generator.cpp
    unittests/test_srs.cpp
    unittests/test_tile.cpp
    unittests/test_tile_conversion.cpp
    unittests/nucleus_tile_scheduler_util.cpp
    unittests/nucleus_tile_scheduler_tile_load_service.cpp
    unittests/nucleus_tile_scheduler_layer_assembler.cpp
    unittests/nucleus_tile_scheduler_quad_assembler.cpp
    unittests/nucleus_tile_scheduler_cache.cpp
    unittests/nucleus_tile_scheduler_scheduler.cpp
    unittests/nucleus_tile_scheduler_slot_limiter.cpp
    unittests/nucleus_tile_scheduler_rate_limiter.cpp
    unittests/RateTester.h unittests/RateTester.cpp
    unittests/test_zppbits.cpp
)

include(FetchContent)
FetchContent_Declare(
  alpine_height_data
  URL      https://gataki.cg.tuwien.ac.at/tiles/alpine_png2/height_data.atb
  DOWNLOAD_NO_EXTRACT true
  URL_HASH MD5=f5458b48ade5d6aaf980fcfc5b6be29a
)
FetchContent_MakeAvailable(alpine_height_data)

########################################### setup #################################################
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if (ALP_ENABLE_POSITIONING)
    find_package(Qt6 REQUIRED COMPONENTS Core Gui OpenGL Network Quick QuickControls2 LinguistTools Svg Positioning Core5Compat)
else()
    find_package(Qt6 REQUIRED COMPONENTS Core Gui OpenGL Network Quick QuickControls2 LinguistTools Svg Core5Compat)
endif()
add_subdirectory(sherpa)    # pulls in Catch2 and glm


if (ALP_ENABLE_ADDRESS_SANITIZER)
    message(NOTICE "building with address sanitizer enabled")
    set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
    set (CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
endif()
if (ALP_ENABLE_THREAD_SANITIZER)
    message(NOTICE "building with thread sanitizer enabled")
    message(WARN ": use the thread sanitizer supression file, e.g.: TSAN_OPTIONS=\"suppressions=thread_sanitizer_suppression.txt\" ./terrainbuilder")
    set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=thread")
    set (CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=thread")
endif()

if (ALP_USE_LLVM_LINKER)
    string(APPEND CMAKE_EXE_LINKER_FLAGS " -fuse-ld=lld")
endif()


FetchContent_Declare(fmt GIT_REPOSITORY https://github.com/fmtlib/fmt.git)
FetchContent_Declare(zppbits GIT_REPOSITORY https://github.com/eyalz800/zpp_bits.git GIT_TAG main)
FetchContent_MakeAvailable(fmt zppbits)
add_library(zppbits INTERFACE)
target_include_directories(zppbits INTERFACE ${zppbits_SOURCE_DIR})


############################################ nucleus ##############################################
qt_add_library(nucleus STATIC
    ${ALP_NUCLEUS_SOURCES}
)

target_include_directories(nucleus PRIVATE . PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(nucleus PUBLIC sherpa Qt::Core Qt::Gui Qt6::Network Qt6::Svg fmt::fmt zppbits)


if (ALP_ENABLE_ASSERTS)
    target_compile_options(nucleus PUBLIC "-U NDEBUG")
endif()
if (ALP_ENABLE_THREADING)
    target_compile_definitions(nucleus PUBLIC "ALP_ENABLE_THREADING")
endif()

########################################### gl engine #############################################
qt_add_library(gl_engine STATIC ${ALP_GL_SOURCES})
target_link_libraries(gl_engine PUBLIC nucleus Qt::OpenGL)
target_include_directories(gl_engine PRIVATE .)

qt_add_resources(gl_engine "gl_shaders"
     PREFIX "/"
     FILES ${ALP_GL_SHADERS}
)
target_compile_definitions(gl_engine PUBLIC "ALP_RESOURCES_PREFIX=\"${CMAKE_SOURCE_DIR}/\"")

########################################### plain render frontend #############################################
qt_add_executable(plain_renderer
    ${ALP_PLAIN_RENDERER_SOURCES}
)
set_target_properties(plain_renderer PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)
target_link_libraries(plain_renderer PUBLIC gl_engine)
target_include_directories(plain_renderer PRIVATE .)

qt_add_resources(plain_renderer "height_data"
    PREFIX "/map"
    BASE ${alpine_height_data_SOURCE_DIR}
    FILES ${alpine_height_data_SOURCE_DIR}/height_data.atb
)

if (EMSCRIPTEN)
    message(NOTICE "ALP_WWW_INSTALL_DIR = ${ALP_WWW_INSTALL_DIR}")
    configure_file(site/plain_renderer.html plain_renderer.html COPYONLY)
    configure_file(site/mascot.png mascot.png COPYONLY)
    set(ALP_INSTALL_FILES
        "$<TARGET_FILE_DIR:plain_renderer>/plain_renderer.js"
        "$<TARGET_FILE_DIR:plain_renderer>/plain_renderer.wasm"
        "$<TARGET_FILE_DIR:plain_renderer>/qtloader.js"
        "${CMAKE_SOURCE_DIR}/site/plain_renderer.html"
        "${CMAKE_SOURCE_DIR}/site/mascot.png"
    )
    if (ALP_ENABLE_THREADING)
        list(APPEND ALP_INSTALL_FILES "$<TARGET_FILE_DIR:plain_renderer>/plain_renderer.worker.js")
    endif()
    install(FILES ${ALP_INSTALL_FILES} DESTINATION ${ALP_WWW_INSTALL_DIR})

endif()

########################################### app render frontend #############################################

qt_add_executable(alpineapp
    ${ALP_APP_SOURCES}
)

qt_add_qml_module(alpineapp
    URI alpinemaps
    VERSION 1.0
    NO_RESOURCE_TARGET_PATH
    QML_FILES ${ALP_APP_QML}
)
qt_add_resources(alpineapp "height_data"
    PREFIX "/map"
    BASE ${alpine_height_data_SOURCE_DIR}
    FILES ${alpine_height_data_SOURCE_DIR}/height_data.atb
)
qt_add_resources(alpineapp "icons"
    BASE "app"
    PREFIX "/"
    FILES ${ALP_APP_ICONS}
)
qt_add_translations(alpineapp TS_FILES app/i18n/de.ts app/i18n/en.ts)

FetchContent_Declare(
  alpineapp_font
  DOWNLOAD_EXTRACT_TIMESTAMP true
  URL      https://github.com/adobe-fonts/source-sans/archive/refs/heads/release.zip
  URL_HASH MD5=681612d5ad33731c512ce056d2ca2297
)
FetchContent_MakeAvailable(alpineapp_font)
qt_add_resources(alpineapp "fonts"
    BASE ${alpineapp_font_SOURCE_DIR}/TTF/
    PREFIX "/fonts"
    FILES
    ${alpineapp_font_SOURCE_DIR}/TTF/SourceSans3-BlackIt.ttf
    ${alpineapp_font_SOURCE_DIR}/TTF/SourceSans3-BoldIt.ttf
    ${alpineapp_font_SOURCE_DIR}/TTF/SourceSans3-ExtraLightIt.ttf
    ${alpineapp_font_SOURCE_DIR}/TTF/SourceSans3-It.ttf
    ${alpineapp_font_SOURCE_DIR}/TTF/SourceSans3-Light.ttf
    ${alpineapp_font_SOURCE_DIR}/TTF/SourceSans3-Medium.ttf
    ${alpineapp_font_SOURCE_DIR}/TTF/SourceSans3-SemiboldIt.ttf
    ${alpineapp_font_SOURCE_DIR}/TTF/SourceSans3-Black.ttf
    ${alpineapp_font_SOURCE_DIR}/TTF/SourceSans3-Bold.ttf
    ${alpineapp_font_SOURCE_DIR}/TTF/SourceSans3-ExtraLight.ttf
    ${alpineapp_font_SOURCE_DIR}/TTF/SourceSans3-LightIt.ttf
    ${alpineapp_font_SOURCE_DIR}/TTF/SourceSans3-MediumIt.ttf
    ${alpineapp_font_SOURCE_DIR}/TTF/SourceSans3-Regular.ttf
    ${alpineapp_font_SOURCE_DIR}/TTF/SourceSans3-Semibold.ttf
)

set_target_properties(alpineapp PROPERTIES
#    MACOSX_BUNDLE_GUI_IDENTIFIER my.example.com
#    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
#    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
#    MACOSX_BUNDLE TRUE
#    WIN32_EXECUTABLE TRUE
    QT_ANDROID_PACKAGE_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/android
)
target_link_libraries(alpineapp PUBLIC gl_engine Qt::Quick Qt::QuickControls2 Qt6::Core5Compat)
if (ALP_ENABLE_POSITIONING)
    target_link_libraries(alpineapp PUBLIC Qt::Positioning)
    target_compile_definitions(alpineapp PUBLIC "ALP_ENABLE_GNSS")
endif()


if (ANDROID)
    FetchContent_Declare(
      android_openssl
      DOWNLOAD_EXTRACT_TIMESTAMP true
      URL      https://github.com/KDAB/android_openssl/archive/refs/heads/master.zip
      URL_HASH MD5=b5c3501237ce9f97dc072c950647d643
    )
    set(PROJECT_NAME alpineapp)
    FetchContent_MakeAvailable(android_openssl)
    set(PROJECT_NAME alpine-renderer)

    install(TARGETS alpineapp
        BUNDLE DESTINATION .
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif()

if (EMSCRIPTEN)
    set(ALP_INSTALL_FILES
        "$<TARGET_FILE_DIR:alpineapp>/alpineapp.js"
        "$<TARGET_FILE_DIR:alpineapp>/alpineapp.wasm"
        "$<TARGET_FILE_DIR:alpineapp>/alpineapp.html"
        "$<TARGET_FILE_DIR:alpineapp>/qtloader.js"
    )
    install(FILES ${ALP_INSTALL_FILES} DESTINATION ${ALP_WWW_INSTALL_DIR})
endif()


#################################### unit tests for backend #######################################
if (ALP_UNITTESTS)
    qt_add_executable(unittests ${ALP_CATCH_UNITTEST_SOURCES})
    qt_add_resources(unittests "test_data"
        PREFIX "/test_data"
        BASE ${CMAKE_SOURCE_DIR}/unittests/data/
        FILES ${ALP_CATCH_UNITTEST_DATA}
    )
    target_link_libraries(unittests PUBLIC nucleus Catch2::Catch2 Qt6::Test)
    target_compile_definitions(unittests PUBLIC "ALP_TEST_DATA_DIR=\":/test_data/\"")
    if (ALP_ENABLE_ASSERTS)
        target_compile_options(unittests PUBLIC "-U NDEBUG")
    endif()

    qt_add_executable(unittests_gl ${ALP_CATCH_UNITTEST_GL_SOURCES})
    qt_add_resources(unittests_gl "gl_shaders"
         PREFIX "/"
         FILES ${ALP_GL_SHADERS}
    )
    target_link_libraries(unittests_gl PUBLIC nucleus Catch2::Catch2 Qt::OpenGL)
    target_compile_definitions(unittests_gl PUBLIC "ALP_RESOURCES_PREFIX=\"${CMAKE_SOURCE_DIR}/\"")
    if (ALP_ENABLE_ASSERTS)
        target_compile_options(unittests_gl PUBLIC "-U NDEBUG")
    endif()

    find_package(Qt6 REQUIRED COMPONENTS Test)
    enable_testing(true)

    if (MINGW)
        target_compile_options(unittests PUBLIC "-Wa,-mbig-obj")
        target_compile_options(unittests_gl PUBLIC "-Wa,-mbig-obj")
    endif()

    if (EMSCRIPTEN)
        target_compile_options(unittests PUBLIC -fexceptions)
        target_link_options(unittests PUBLIC -sASYNCIFY -Os -fexceptions)
#        target_link_options(unittests PUBLIC --shell-file ${CMAKE_SOURCE_DIR}/site/unittest_shell.html)
#        target_link_options(unittests_gl PUBLIC --shell-file ${CMAKE_SOURCE_DIR}/site/unittest_shell.html)
#        set_target_properties(unittests unittests_gl PROPERTIES SUFFIX ".html")


        configure_file(site/unittests.html unittests.html COPYONLY)
        configure_file(site/unittests_gl.html unittests_gl.html COPYONLY)
        configure_file(site/ansispan.js ansispan.js COPYONLY)
        set(ALP_INSTALL_FILES
            "$<TARGET_FILE_DIR:unittests>/unittests.js"
            "$<TARGET_FILE_DIR:unittests>/unittests.wasm"
            "$<TARGET_FILE_DIR:unittests>/qtloader.js"
            "$<TARGET_FILE_DIR:unittests_gl>/unittests_gl.js"
            "$<TARGET_FILE_DIR:unittests_gl>/unittests_gl.wasm"
            "$<TARGET_FILE_DIR:unittests_gl>/qtloader.js"
            "${CMAKE_SOURCE_DIR}/site/unittests.html"
            "${CMAKE_SOURCE_DIR}/site/unittests_gl.html"
            "${CMAKE_SOURCE_DIR}/site/ansispan.js"
            "${CMAKE_SOURCE_DIR}/site/mascot.png"
        )

#        set(ALP_INSTALL_FILES
#            "$<TARGET_FILE_DIR:unittests>/unittests.js"
#            "$<TARGET_FILE_DIR:unittests>/unittests.wasm"
#            "$<TARGET_FILE_DIR:unittests>/unittests.html"
#            "$<TARGET_FILE_DIR:unittests_gl>/unittests_gl.js"
#            "$<TARGET_FILE_DIR:unittests_gl>/unittests_gl.wasm"
#            "$<TARGET_FILE_DIR:unittests_gl>/unittests_gl.html"
#        )
        install(FILES ${ALP_INSTALL_FILES} DESTINATION ${ALP_WWW_INSTALL_DIR})
    endif()

endif()
