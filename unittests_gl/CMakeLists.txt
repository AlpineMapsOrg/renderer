cmake_minimum_required(VERSION 3.21)
project(alpine-renderer-unittests_gl_engine LANGUAGES CXX)

qt_add_executable(unittests_gl_engine 
    UnittestGlWindow.h UnittestGlWindow.cpp
    main.cpp
    framebuffer.cpp
)

target_link_libraries(unittests_gl_engine PUBLIC gl_engine Catch2::Catch2)

alp_setup_unittest(unittests_gl_engine)

if (EMSCRIPTEN)
    configure_file(${CMAKE_SOURCE_DIR}/site/unittests_gl.html unittests_gl.html COPYONLY)
    configure_file(${CMAKE_SOURCE_DIR}/site/ansispan.js ansispan.js COPYONLY)

    install(FILES
        "$<TARGET_FILE_DIR:unittests_gl_engine>/unittests_gl_engine.js"
        "$<TARGET_FILE_DIR:unittests_gl_engine>/unittests_gl_engine.wasm"
        "$<TARGET_FILE_DIR:unittests_gl_engine>/qtloader.js"
        "${CMAKE_SOURCE_DIR}/site/unittests_gl.html"
        "${CMAKE_SOURCE_DIR}/site/ansispan.js"
        "${CMAKE_SOURCE_DIR}/site/mascot.png"
        DESTINATION ${ALP_WWW_INSTALL_DIR})

    if (ALP_ENABLE_THREADING)
        install(FILES "$<TARGET_FILE_DIR:unittests_gl_engine>/unittests_gl_engine.worker.js" DESTINATION ${ALP_WWW_INSTALL_DIR})
    endif()
endif()
