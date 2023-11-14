cmake_minimum_required(VERSION 3.24)
project(alpine-renderer LANGUAGES CXX)

option(ALP_UNITTESTS "include unit test targets in the buildsystem" ON)
option(ALP_ENABLE_ADDRESS_SANITIZER "compiles atb with address sanitizer enabled (only debug, works only on g++ and clang)" OFF)
option(ALP_ENABLE_THREAD_SANITIZER "compiles atb with thread sanitizer enabled (only debug, works only on g++ and clang)" OFF)
option(ALP_ENABLE_ASSERTS "enable asserts (do not define NDEBUG)" ON)
option(ALP_ENABLE_TRACK_OBJECT_LIFECYCLE "enables debug cmd printout of constructors & deconstructors if implemented" OFF)
option(ALP_USE_NOISY_CMAKE_DOWNLOAD "we download quite a lot when running cmake the first time. if this option is off, it may look like cmake stalled." ON)

set(ALP_EXTERN_DIR "extern" CACHE STRING "name of the directory to store external libraries, fonts etc..")

if(ALP_ENABLE_TRACK_OBJECT_LIFECYCLE)
    add_definitions(-DALP_ENABLE_TRACK_OBJECT_LIFECYCLE)
endif()

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

########################################### setup #################################################
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if (ALP_USE_NOISY_CMAKE_DOWNLOAD)
    set(FETCHCONTENT_QUIET FALSE)
else()
    set(FETCHCONTENT_QUIET TRUE)
endif()

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
find_package(Qt6 REQUIRED COMPONENTS Core Gui OpenGL Network Quick QuickControls2 LinguistTools Svg)
if (ALP_ENABLE_POSITIONING)
    find_package(Qt6 REQUIRED COMPONENTS Positioning)
endif()
if (ALP_UNITTESTS)
    find_package(Qt6 REQUIRED COMPONENTS Test)
endif()

include(FetchContent)
FetchContent_Declare(alpine_height_data
    DOWNLOAD_NO_EXTRACT true
    URL             https://gataki.cg.tuwien.ac.at/tiles/alpine_png2/height_data.atb
    URL_HASH        MD5=f5458b48ade5d6aaf980fcfc5b6be29a
    SOURCE_DIR      ${CMAKE_SOURCE_DIR}/${ALP_EXTERN_DIR}/alpine_height_data
)
FetchContent_Declare(alpineapp_fonts
  DOWNLOAD_EXTRACT_TIMESTAMP true
  URL               https://github.com/adobe-fonts/source-sans/archive/refs/heads/release.zip
  URL_HASH          MD5=681612d5ad33731c512ce056d2ca2297
  SOURCE_DIR        ${CMAKE_SOURCE_DIR}/${ALP_EXTERN_DIR}/alpineapp_fonts
)
FetchContent_MakeAvailable(alpine_height_data alpineapp_fonts)

if (ANDROID)
    FetchContent_Declare(
      android_openssl
      URL      https://github.com/AlpineMapsOrg/android_openssl/archive/refs/heads/issams_fixes.zip
#      URL_HASH MD5=88dd38c0fc94a195ae3f29412cfdcf2b
    )
    FetchContent_MakeAvailable(android_openssl)
    include(${android_openssl_SOURCE_DIR}/android_openssl.cmake)
endif()



########################################### projects #################################################
add_subdirectory(nucleus)
add_subdirectory(gl_engine)
add_subdirectory(plain_renderer)
add_subdirectory(app)

if (ALP_UNITTESTS)
    if (EMSCRIPTEN AND ALP_ENABLE_THREADING)
        target_compile_options(Catch2 PRIVATE -pthread)
    endif()

    function(alp_setup_unittest)
        if (ANDROID)
            install(TARGETS ${ARGV0}
                LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
                RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
        endif()

        if (MINGW)
            target_compile_options(${ARGV0} PUBLIC "-Wa,-mbig-obj")
        endif()
    endfunction()

    add_subdirectory(unittests)
    add_subdirectory(unittests_gl)
endif()
