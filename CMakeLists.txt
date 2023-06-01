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
set(ALP_CATCH_UNITTEST_GL_SOURCES
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

########################################### setup #################################################
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)


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

########################################### dependencies #################################################
find_package(Qt6 REQUIRED COMPONENTS Core Gui OpenGL Network Quick QuickControls2 LinguistTools Svg Core5Compat)
if (ALP_ENABLE_POSITIONING)
    find_package(Qt6 REQUIRED COMPONENTS Positioning)
endif()
if (ALP_UNITTESTS)
    find_package(Qt6 REQUIRED COMPONENTS Test)
endif()


include(FetchContent)
FetchContent_Declare(
  alpine_height_data
  URL      https://gataki.cg.tuwien.ac.at/tiles/alpine_png2/height_data.atb
  DOWNLOAD_NO_EXTRACT true
  URL_HASH MD5=f5458b48ade5d6aaf980fcfc5b6be29a
)
FetchContent_Declare(
  alpineapp_fonts
  DOWNLOAD_EXTRACT_TIMESTAMP true
  URL      https://github.com/adobe-fonts/source-sans/archive/refs/heads/release.zip
  URL_HASH MD5=681612d5ad33731c512ce056d2ca2297
)
FetchContent_Declare(fmt GIT_REPOSITORY https://github.com/fmtlib/fmt.git)
FetchContent_Declare(zppbits GIT_REPOSITORY https://github.com/eyalz800/zpp_bits.git GIT_TAG main)

FetchContent_MakeAvailable(alpine_height_data alpineapp_fonts fmt zppbits)

if (ANDROID)
    FetchContent_Declare(
      android_openssl
      DOWNLOAD_EXTRACT_TIMESTAMP true
      URL      https://github.com/AlpineMapsOrg/android_openssl/archive/refs/heads/master.zip
      URL_HASH MD5=3bc2ef11f914784089f58f1e7e3024e7
    )
    FetchContent_MakeAvailable(android_openssl)
endif()

add_library(zppbits INTERFACE)
target_include_directories(zppbits INTERFACE ${zppbits_SOURCE_DIR})


add_subdirectory(sherpa)    # pulls in Catch2 and glm
add_subdirectory(nucleus)
add_subdirectory(gl_engine)
add_subdirectory(plain_renderer)
add_subdirectory(app)


#################################### unit tests for backend #######################################
if (ALP_UNITTESTS)
    if (EMSCRIPTEN AND ALP_ENABLE_THREADING)
        target_compile_options(Catch2 PRIVATE -pthread)
    endif()

    qt_add_executable(unittests ${ALP_CATCH_UNITTEST_SOURCES})
    qt_add_resources(unittests "test_data"
        PREFIX "/test_data"
        BASE ${CMAKE_SOURCE_DIR}/unittests/data/
        FILES ${ALP_CATCH_UNITTEST_DATA}
    )
    target_link_libraries(unittests PUBLIC nucleus Catch2::Catch2 Qt::Test)
    target_compile_definitions(unittests PUBLIC "ALP_TEST_DATA_DIR=\":/test_data/\"")
    if (ALP_ENABLE_ASSERTS)
        target_compile_options(unittests PUBLIC "-U NDEBUG")
    endif()

    qt_add_executable(unittests_gl ${ALP_CATCH_UNITTEST_GL_SOURCES})
    target_link_libraries(unittests_gl PUBLIC nucleus gl_engine Catch2::Catch2)
    if (ALP_ENABLE_ASSERTS)
        target_compile_options(unittests_gl PUBLIC "-U NDEBUG")
    endif()

    enable_testing(true)

    if (ANDROID)
        android_add_openssl_to_targets(alpineapp)

        install(TARGETS unittests
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
        install(TARGETS unittests_gl
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    endif()

    if (MINGW)
        target_compile_options(unittests PUBLIC "-Wa,-mbig-obj")
        target_compile_options(unittests_gl PUBLIC "-Wa,-mbig-obj")
    endif()

    if (EMSCRIPTEN)
        # asyncify required for running nested event loops in some of the tests
        target_link_options(unittests PUBLIC -sASYNCIFY -Os)

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
        install(FILES ${ALP_INSTALL_FILES} DESTINATION ${ALP_WWW_INSTALL_DIR})
    endif()

endif()
