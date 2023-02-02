cmake_minimum_required(VERSION 3.21)
project(alpine-renderer LANGUAGES CXX)

option(ALP_UNITTESTS "include unit test targets in the buildsystem" ON)
option(ALP_ENABLE_ADDRESS_SANITIZER "compiles atb with address sanitizer enabled (only debug, works only on g++ and clang)" OFF)
option(ALP_ENABLE_THREAD_SANITIZER "compiles atb with thread sanitizer enabled (only debug, works only on g++ and clang)" OFF)
option(ALP_ENABLE_THREADING "Puts the scheduler into an extra thread." OFF)
option(ALP_ENABLE_ASSERTS "enable asserts (do not define NDEBUG)" ON)
set(ALP_INSTALL_DIR "${CMAKE_CURRENT_BINARY_DIR}" CACHE PATH "path to the install directory (for webassembly files, i.e., www directory)")
option(ALP_USE_LLVM_LINKER "use lld (llvm) for linking. it's parallel and much faster, but not installed by default. if it's not installed, you'll get errors, that openmp or other stuff is not installed (hard to track down)" OFF)


############################################ sources ##############################################
set(ALP_NUCLEUS_SOURCES
    nucleus/AbstractRenderWindow.h nucleus/AbstractRenderWindow.cpp
    nucleus/Controller.h nucleus/Controller.cpp
    nucleus/Raster.h
    nucleus/srs.h nucleus/srs.cpp
    nucleus/Tile.cpp nucleus/Tile.h
    nucleus/TileScheduler.h nucleus/TileScheduler.cpp
    nucleus/tile_scheduler/utils.h
    nucleus/tile_scheduler/DrawListGenerator.h nucleus/tile_scheduler/DrawListGenerator.cpp
    nucleus/tile_scheduler/GpuCacheTileScheduler.h nucleus/tile_scheduler/GpuCacheTileScheduler.cpp
    nucleus/tile_scheduler/SimplisticTileScheduler.h nucleus/tile_scheduler/SimplisticTileScheduler.cpp
    nucleus/TileLoadService.h nucleus/TileLoadService.cpp
    nucleus/camera/Controller.h nucleus/camera/Controller.cpp
    nucleus/camera/CrapyInteraction.h nucleus/camera/CrapyInteraction.cpp
    nucleus/camera/Definition.h nucleus/camera/Definition.cpp
    nucleus/camera/InteractionStyle.h nucleus/camera/InteractionStyle.cpp
    nucleus/camera/NearPlaneAdjuster.h nucleus/camera/NearPlaneAdjuster.cpp
    nucleus/camera/OrbitInteraction.h nucleus/camera/OrbitInteraction.cpp
    nucleus/camera/stored_positions.h
    nucleus/utils/terrain_mesh_index_generator.h
    nucleus/utils/tile_conversion.h nucleus/utils/tile_conversion.cpp
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
set(ALP_PLAIN_RENDERER_SOURCES
    plain_renderer/main.cpp
    plain_renderer/Window.h plain_renderer/Window.cpp
)
set(ALP_GL_SHADERS
    gl_shaders/atmosphere_bg.frag
    gl_shaders/atmosphere_implementation.frag
    gl_shaders/screen_copy.frag
    gl_shaders/screen_pass.vert
    gl_shaders/tile.frag
    gl_shaders/tile.vert
)
if (ALP_UNITTESTS AND NOT EMSCRIPTEN)
    set(ALP_CATCH_UNITTEST_GL_SOURCES
        gl_engine/Framebuffer.h gl_engine/Framebuffer.cpp
        gl_engine/ShaderProgram.h gl_engine/ShaderProgram.cpp
        unittests_gl/main.cpp
        unittests_gl/framebuffer.cpp
    )
    set(ALP_CATCH_UNITTEST_SOURCES
        unittests/main.cpp
        unittests/catch2_helpers.h
        unittests/test_Camera.cpp
        unittests/test_DrawListGenerator.cpp
        unittests/test_helpers.h
        unittests/test_raster.cpp
        unittests/test_terrain_mesh_index_generator.cpp
        unittests/test_srs.cpp
        unittests/test_tile.cpp
        unittests/test_tile_conversion.cpp
    )
    set(ALP_QT_UNITTESTS
        TileLoadService
        camera_Controller_and_NearPlaneAdjuster
    )
    set(ALP_QT_SCHEDULER_UNITTESTS
        SimplisticTileScheduler
        GpuCacheTileScheduler
    )
endif()


########################################### setup #################################################
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui OpenGL Network)
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

############################################ nucleus ##############################################
qt_add_library(nucleus STATIC
    ${ALP_NUCLEUS_SOURCES}
)

target_include_directories(nucleus PRIVATE . PUBLIC ${CMAKE_SOURCE_DIR})
#target_compile_definitions(nucleus PUBLIC GLM_ENABLE_EXPERIMENTAL)
if (NOT EMSCRIPTEN)
    target_compile_definitions(nucleus PUBLIC ALP_USE_DISK_CACHE)
endif()
target_link_libraries(nucleus PUBLIC sherpa Qt::Core Qt::Gui Qt6::Network)


if (ALP_ENABLE_ASSERTS)
    target_compile_options(nucleus PUBLIC "-U NDEBUG")
endif()
if (ALP_ENABLE_THREADING)
    target_compile_definitions(nucleus PUBLIC "ALP_ENABLE_THREADING")
endif()

########################################### gl engine #############################################
qt_add_library(gl_engine STATIC ${ALP_GL_SOURCES})
target_link_libraries(gl_engine PUBLIC nucleus Qt::Core Qt::Gui Qt::OpenGL)
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
target_link_libraries(plain_renderer PUBLIC
    gl_engine
)
target_include_directories(plain_renderer PRIVATE .)

message(NOTICE "ALP_INSTALL_DIR = ${ALP_INSTALL_DIR}")
if (NOT EMSCRIPTEN)
    install(TARGETS plain_renderer
        RUNTIME DESTINATION "${ALP_INSTALL_DIR}"
        BUNDLE DESTINATION "${ALP_INSTALL_DIR}"
        LIBRARY DESTINATION "${ALP_INSTALL_DIR}"
    )
else ()
    configure_file(site/index.html index.html COPYONLY)
    configure_file(site/mascot.png mascot.png COPYONLY)
    set(ALP_INSTALL_FILES
        "$<TARGET_FILE_DIR:plain_renderer>/plain_renderer.js"
        "$<TARGET_FILE_DIR:plain_renderer>/plain_renderer.wasm"
        "$<TARGET_FILE_DIR:plain_renderer>/qtloader.js"
        "${CMAKE_SOURCE_DIR}/site/index.html"
        "${CMAKE_SOURCE_DIR}/site/mascot.png"
    )
    if (ALP_ENABLE_THREADING)
        list(APPEND ALP_INSTALL_FILES "$<TARGET_FILE_DIR:plain_renderer>/plain_renderer.worker.js")
    endif()
    install(FILES ${ALP_INSTALL_FILES} DESTINATION ${ALP_INSTALL_DIR})

endif()

#################################### unit tests for backend #######################################
if (ALP_UNITTESTS AND NOT EMSCRIPTEN)
    add_executable(unittests ${ALP_CATCH_UNITTEST_SOURCES})
    target_link_libraries(unittests PUBLIC nucleus Catch2::Catch2)
    target_compile_definitions(unittests PUBLIC "ALP_TEST_DATA_DIR=\"${CMAKE_SOURCE_DIR}/unittests/data/\"")
    if (ALP_ENABLE_ASSERTS)
        target_compile_options(unittests PUBLIC "-U NDEBUG")
    endif()

    qt_add_executable(unittests_gl ${ALP_CATCH_UNITTEST_GL_SOURCES})
    target_link_libraries(unittests_gl PUBLIC nucleus Catch2::Catch2 Qt::Core Qt::Gui Qt::OpenGL)
    target_compile_definitions(unittests_gl PUBLIC "ALP_RESOURCES_PREFIX=\"${CMAKE_SOURCE_DIR}/\"")
    if (ALP_ENABLE_ASSERTS)
        target_compile_options(unittests_gl PUBLIC "-U NDEBUG")
    endif()

    find_package(Qt6 REQUIRED COMPONENTS Test)
    enable_testing(true)
    function(add_cute_test name)
        add_executable(qtest_${name} unittests_qt/${name}.cpp)
        add_test(NAME ${name} COMMAND qtest_${name})
        target_link_libraries(qtest_${name} PUBLIC nucleus Qt6::Test)
        target_compile_definitions(qtest_${name} PUBLIC "ALP_TEST_DATA_DIR=\"${CMAKE_SOURCE_DIR}/unittests/data/\"")
    endfunction()
    foreach(cute_test ${ALP_QT_UNITTESTS})
        add_cute_test(${cute_test})
    endforeach()


    function(add_cute_scheduler_test name)
        add_executable(qtest_${name} unittests_qt/${name}.cpp unittests_qt/TileScheduler.h)
        add_test(NAME qtest_${name} COMMAND ${name})
        target_link_libraries(qtest_${name} PUBLIC nucleus Qt6::Test)
        target_compile_definitions(qtest_${name} PUBLIC "ALP_TEST_DATA_DIR=\"${CMAKE_SOURCE_DIR}/unittests/data/\"")
    endfunction()
    foreach(cute_test ${ALP_QT_SCHEDULER_UNITTESTS})
        add_cute_scheduler_test(${cute_test})
    endforeach()

endif()
