cmake_minimum_required(VERSION 3.21)
project(alpine-renderer LANGUAGES CXX)

option(ATB_UNITTESTS "include unit test targets in the buildsystem" ON)
option(ATB_ENABLE_ADDRESS_SANITIZER "compiles atb with address sanitizer enabled (only debug, works only on g++ and clang)" ON)
option(ATB_ENABLE_THREAD_SANITIZER "compiles atb with thread sanitizer enabled (only debug, works only on g++ and clang)" OFF)
option(ATB_ENABLE_ASSERTS "enable asserts (do not define NDEBUG)" ON)
set(ATB_INSTALL_DIR CACHE PATH "path to the install directory (for webassembly files, i.e., www directory)")
option(ATB_USE_LLVM_LINKER "use lld (llvm) for linking. it's parallel and much faster, but not installed by default. if it's not installed, you'll get errors, that openmp or other stuff is not installed (hard to track down)" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)


if (ATB_UNITTESTS AND NOT EMSCRIPTEN)
    find_package(Catch2 REQUIRED)
endif()
find_package(Qt6 COMPONENTS Core)
find_package(Qt6 COMPONENTS Gui)
find_package(Qt6 COMPONENTS OpenGL)

if (ATB_ENABLE_ADDRESS_SANITIZER)
    message(NOTICE "building with address sanitizer enabled")
    set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
    set (CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
endif()
if (ATB_ENABLE_THREAD_SANITIZER)
    message(NOTICE "building with thread sanitizer enabled")
    message(WARN ": use the thread sanitizer supression file, e.g.: TSAN_OPTIONS=\"suppressions=thread_sanitizer_suppression.txt\" ./terrainbuilder")
    set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=thread")
    set (CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=thread")
endif()

if (ATB_USE_LLVM_LINKER)
    string(APPEND CMAKE_EXE_LINKER_FLAGS " -fuse-ld=lld")
endif()

############################################ backend ##############################################
set(ATB_RENDER_BACKEND_SOURCES
    render_backend/Raster.h
    render_backend/Tile.cpp render_backend/Tile.h
)
add_library(render_backend STATIC
    ${ATB_RENDER_BACKEND_SOURCES}
)
target_compile_definitions(render_backend PUBLIC GLM_FORCE_SWIZZLE GLM_ENABLE_EXPERIMENTAL GLM_FORCE_XYZW_ONLY)
target_link_libraries(render_backend PUBLIC Qt::Core)


if (ATB_ENABLE_ASSERTS)
    target_compile_options(render_backend PUBLIC "-U NDEBUG")
endif()

########################################### gl engine #############################################

set(ATB_GL_ENGINE_SOURCES
    alpine_gl_renderer/glwindow.cpp alpine_gl_renderer/glwindow.h
    alpine_gl_renderer/main.cpp
)
add_executable(alpine_gl_renderer
    ${ATB_GL_ENGINE_SOURCES}
)
set_target_properties(alpine_gl_renderer PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)
target_link_libraries(alpine_gl_renderer PUBLIC
    render_backend
    Qt::Core
    Qt::Gui
    Qt::OpenGL
)

if (ATB_ENABLE_ASSERTS)
    target_compile_options(alpine_gl_renderer PUBLIC "-U NDEBUG")
endif()

# Resources:
#set(ATB_RESOURCES
    #"logo.png"
#)

#qt6_add_resources(alpinerenderengine "alpinerenderengine"
    #PREFIX
        #"/"
    #FILES
        #${ATB_RESOURCES}
#)
message(NOTICE "ATB_INSTALL_DIR = ${ATB_INSTALL_DIR}")
if (NOT EMSCRIPTEN)
    install(TARGETS alpine_gl_renderer
        RUNTIME DESTINATION "${ATB_INSTALL_DIR}"
        BUNDLE DESTINATION "${ATB_INSTALL_DIR}"
        LIBRARY DESTINATION "${ATB_INSTALL_DIR}"
    )
else ()
    install(FILES
        "$<TARGET_FILE_DIR:alpine_gl_renderer>/alpine_gl_renderer.js"
        "$<TARGET_FILE_DIR:alpine_gl_renderer>/alpine_gl_renderer.wasm"
        DESTINATION ${ATB_INSTALL_DIR})
endif()


#################################### unit tests for backend #######################################
if (ATB_UNITTESTS AND NOT EMSCRIPTEN)
    set(ATB_UNITTEST_SOURCES
        unittests/test_helpers.h
        unittests/test_raster.cpp
        unittests/main.cpp
    )
    add_executable(unittests ${ATB_UNITTEST_SOURCES})
    target_link_libraries(unittests PUBLIC render_backend Catch2::Catch2)
    if (ATB_ENABLE_ASSERTS)
        target_compile_options(unittests PUBLIC "-U NDEBUG")
    endif()
endif()
