cmake_minimum_required(VERSION 3.21)
project(alpine-renderer-gl_engine LANGUAGES CXX)

qt_add_library(gl_engine STATIC
    Atmosphere.h Atmosphere.cpp
    Framebuffer.h Framebuffer.cpp
    ShaderManager.h ShaderManager.cpp
    TileManager.h TileManager.cpp
    TileSet.h
    Window.cpp Window.h
    DebugPainter.h DebugPainter.cpp
    helpers.h
    ShaderProgram.h ShaderProgram.cpp
)
target_link_libraries(gl_engine PUBLIC nucleus Qt::OpenGL)
target_include_directories(gl_engine PRIVATE .)

qt_add_resources(gl_engine "shaders"
    PREFIX "/gl_shaders"
    BASE "shaders/"
    FILES
    shaders/atmosphere_bg.frag
    shaders/atmosphere_implementation.frag
    shaders/screen_copy.frag
    shaders/screen_pass.vert
    shaders/tile.frag
    shaders/tile.vert
    shaders/depth.frag
    shaders/encoder.glsl
)
target_compile_definitions(gl_engine PUBLIC ALP_RESOURCES_PREFIX="${CMAKE_CURRENT_SOURCE_DIR}/shaders/")
