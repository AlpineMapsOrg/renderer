cmake_minimum_required(VERSION 3.21)
project(alpine-renderer-gl_engine LANGUAGES CXX)

# If true the shaders will be loaded from the given ALP_SHADER_NETWORK_URL and can be reloaded
# inside the APP without the need for recompilation. You have to serve the files yourself. There is no HTTP-Server!
option(ALP_ENABLE_SHADER_NETWORK_HOTRELOAD "reload shader files per HTTP request. Allows Hot-Reload on Android/WebGL" OFF)
set(ALP_SHADER_NETWORK_URL "http://localhost:5500/" CACHE STRING "url location, which points to the shaders directory for network hot reload")

#http://localhost:5500/
qt_add_library(gl_engine STATIC
    Framebuffer.h Framebuffer.cpp
    ShaderManager.h ShaderManager.cpp
    TileManager.h TileManager.cpp
    TileSet.h
    Window.cpp Window.h
    DebugPainter.h DebugPainter.cpp
    helpers.h
    ShaderProgram.h ShaderProgram.cpp
    UniformBufferObjects.h UniformBufferObjects.cpp
    UniformBuffer.h UniformBuffer.cpp
    SSAO.h SSAO.cpp
    ShadowMapping.h ShadowMapping.cpp
    GpuAsyncQueryTimer.h GpuAsyncQueryTimer.cpp
    PolyLine.h PolyLine.cpp
    TrackManager.h TrackManager.cpp
)
target_link_libraries(gl_engine PUBLIC nucleus Qt::OpenGL)
target_include_directories(gl_engine PRIVATE .)

qt_add_resources(gl_engine "shaders"
    PREFIX "/gl_shaders"
    BASE "shaders/"
    FILES
    shaders/atmosphere_bg.frag
    shaders/atmosphere_implementation.glsl
    shaders/screen_copy.frag
    shaders/screen_pass.vert
    shaders/tile.frag
    shaders/tile.vert
    shaders/encoder.glsl
    shaders/compose.frag
    shaders/shared_config.glsl
    shaders/camera_config.glsl
    shaders/hashing.glsl
    shaders/ssao.frag
    shaders/ssao_blur.frag
    shaders/shadowmap.vert
    shaders/shadowmap.frag
    shaders/shadow_config.glsl
    shaders/overlay_steepness.glsl
    shaders/polyline.vert
    shaders/polyline.frag
)
target_compile_definitions(gl_engine PUBLIC ALP_RESOURCES_PREFIX="${CMAKE_CURRENT_SOURCE_DIR}/shaders/")

if(ALP_ENABLE_SHADER_NETWORK_HOTRELOAD)
    target_compile_definitions(gl_engine PUBLIC ALP_ENABLE_SHADER_NETWORK_HOTRELOAD=true)
else()
    target_compile_definitions(gl_engine PUBLIC ALP_ENABLE_SHADER_NETWORK_HOTRELOAD=false)
endif()

#target_compile_definitions(gl_engine PUBLIC ALP_SHADER_NETWORK_URL="http://localhost:5500/")

target_compile_definitions(gl_engine PUBLIC "ALP_SHADER_NETWORK_URL=\"${ALP_SHADER_NETWORK_URL}\"")
