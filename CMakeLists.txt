cmake_minimum_required(VERSION 3.21)
project(alpine-renderer LANGUAGES CXX)

option(ATB_UNITTESTS "include unit test targets in the buildsystem" ON)
option(ATB_ENABLE_ADDRESS_SANITIZER "compiles atb with address sanitizer enabled (only debug, works only on g++ and clang)" ON)
option(ATB_ENABLE_THREAD_SANITIZER "compiles atb with thread sanitizer enabled (only debug, works only on g++ and clang)" OFF)
option(ATB_ENABLE_ASSERTS "enable asserts (do not define NDEBUG)" ON)
set(ATB_INSTALL_DIR "${CMAKE_CURRENT_BINARY_DIR}" CACHE PATH "path to the install directory (for webassembly files, i.e., www directory)")
option(ATB_USE_LLVM_LINKER "use lld (llvm) for linking. it's parallel and much faster, but not installed by default. if it's not installed, you'll get errors, that openmp or other stuff is not installed (hard to track down)" OFF)


############################################ sources ##############################################
set(ATB_RENDER_BACKEND_SOURCES
    nucleus/Raster.h
    nucleus/srs.h nucleus/srs.cpp
    nucleus/Tile.cpp nucleus/Tile.h
    nucleus/TileScheduler.h nucleus/TileScheduler.cpp
    nucleus/tile_scheduler/utils.h
    nucleus/tile_scheduler/SimplisticTileScheduler.h nucleus/tile_scheduler/SimplisticTileScheduler.cpp
    nucleus/tile_scheduler/BasicTreeTileScheduler.h nucleus/tile_scheduler/BasicTreeTileScheduler.cpp
    nucleus/TileLoadService.h nucleus/TileLoadService.cpp
    nucleus/camera/Controller.h nucleus/camera/Controller.cpp
    nucleus/camera/CrapyInteraction.h nucleus/camera/CrapyInteraction.cpp
    nucleus/camera/Definition.h nucleus/camera/Definition.cpp
    nucleus/camera/InteractionStyle.h nucleus/camera/InteractionStyle.cpp
    nucleus/camera/NearPlaneAdjuster.h nucleus/camera/NearPlaneAdjuster.cpp
    nucleus/utils/QuadTree.h
    nucleus/utils/terrain_mesh_index_generator.h
    nucleus/utils/tile_conversion.h nucleus/utils/tile_conversion.cpp
)
set(ATB_GL_ENGINE_SOURCES
    alpine_gl_renderer/GLShaderManager.h alpine_gl_renderer/GLShaderManager.cpp
    alpine_gl_renderer/GLTileManager.h alpine_gl_renderer/GLTileManager.cpp
    alpine_gl_renderer/GLTileSet.h
    alpine_gl_renderer/GLWindow.cpp alpine_gl_renderer/GLWindow.h
    alpine_gl_renderer/GLDebugPainter.h alpine_gl_renderer/GLDebugPainter.cpp
    alpine_gl_renderer/GLHelpers.h
    alpine_gl_renderer/ShaderProgram.h alpine_gl_renderer/ShaderProgram.cpp
    alpine_gl_renderer/main.cpp
)
set(ATB_GL_SHADERS
    gl_shaders/screen_pass.vert
    gl_shaders/screen_copy.frag
)
if (ATB_UNITTESTS AND NOT EMSCRIPTEN)
    set(ATB_CATCH_UNITTEST_SOURCES
        unittests/main.cpp
        unittests/catch2_helpers.h
        unittests/test_Camera.cpp
        unittests/test_helpers.h
        unittests/test_QuadTree.cpp
        unittests/test_raster.cpp
        unittests/test_terrain_mesh_index_generator.cpp
        unittests/test_srs.cpp
        unittests/test_tile.cpp
        unittests/test_tile_conversion.cpp
    )
    set(ATB_QT_UNITTESTS
        TileLoadService
        camera_Controller_and_NearPlaneAdjuster
    )
    set(ATB_QT_SCHEDULER_UNITTESTS
        BasicTreeTileScheduler
        SimplisticTileScheduler
    )
endif()


########################################### setup #################################################
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if (ATB_UNITTESTS AND NOT EMSCRIPTEN)
    find_package(Catch2 REQUIRED)
endif()
find_package(Qt6 REQUIRED COMPONENTS Core Gui OpenGL Network)
add_subdirectory(sherpa)


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
qt_add_library(nucleus STATIC
    ${ATB_RENDER_BACKEND_SOURCES}
)

target_include_directories(nucleus SYSTEM PUBLIC ${CMAKE_SOURCE_DIR}/libs/glm)
target_include_directories(nucleus PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(nucleus PUBLIC GLM_ENABLE_EXPERIMENTAL GLM_FORCE_XYZW_ONLY)
target_link_libraries(nucleus PUBLIC sherpa Qt::Core Qt::Gui Qt6::Network)


if (ATB_ENABLE_ASSERTS)
    target_compile_options(nucleus PUBLIC "-U NDEBUG")
endif()

########################################### gl engine #############################################
qt_add_executable(alpine_gl_renderer
    ${ATB_GL_ENGINE_SOURCES}
)
set_target_properties(alpine_gl_renderer PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)
target_link_libraries(alpine_gl_renderer PUBLIC
    nucleus
    Qt::Core
    Qt::Gui
    Qt::OpenGL
)

if (ATB_ENABLE_ASSERTS)
    target_compile_options(alpine_gl_renderer PUBLIC "-U NDEBUG")
endif()

qt_add_resources(alpine_gl_renderer "gl_shaders"
     PREFIX "/"
     FILES ${ATB_GL_SHADERS}
)

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
    add_executable(unittests ${ATB_CATCH_UNITTEST_SOURCES})
    target_link_libraries(unittests PUBLIC nucleus Catch2::Catch2)
    target_compile_definitions(unittests PUBLIC "ATB_TEST_DATA_DIR=\"${CMAKE_SOURCE_DIR}/unittests/data/\"")
    if (ATB_ENABLE_ASSERTS)
        target_compile_options(unittests PUBLIC "-U NDEBUG")
    endif()

    find_package(Qt6 REQUIRED COMPONENTS Test)
    enable_testing(true)
    function(add_cute_test name)
        add_executable(qtest_${name} unittests_qt/${name}.cpp)
        add_test(NAME ${name} COMMAND qtest_${name})
        target_link_libraries(qtest_${name} PUBLIC nucleus Qt6::Test)
        target_compile_definitions(qtest_${name} PUBLIC "ATB_TEST_DATA_DIR=\"${CMAKE_SOURCE_DIR}/unittests/data/\"")
    endfunction()
    foreach(cute_test ${ATB_QT_UNITTESTS})
        add_cute_test(${cute_test})
    endforeach()


    function(add_cute_scheduler_test name)
        add_executable(qtest_${name} unittests_qt/${name}.cpp unittests_qt/TileScheduler.h)
        add_test(NAME qtest_${name} COMMAND ${name})
        target_link_libraries(qtest_${name} PUBLIC nucleus Qt6::Test)
        target_compile_definitions(qtest_${name} PUBLIC "ATB_TEST_DATA_DIR=\"${CMAKE_SOURCE_DIR}/unittests/data/\"")
    endfunction()
    foreach(cute_test ${ATB_QT_SCHEDULER_UNITTESTS})
        add_cute_scheduler_test(${cute_test})
    endforeach()

endif()
