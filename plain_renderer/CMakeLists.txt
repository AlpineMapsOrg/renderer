cmake_minimum_required(VERSION 3.21)
project(alpine-renderer-plain_renderer LANGUAGES CXX)

qt_add_executable(plain_renderer
    main.cpp
    Window.h Window.cpp
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
    configure_file(../site/plain_renderer.html plain_renderer.html COPYONLY)
    configure_file(../site/mascot.png mascot.png COPYONLY)
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
