cmake_minimum_required(VERSION 3.21)
project(alpine-renderer-unittests_nucleus LANGUAGES CXX)

qt_add_executable(unittests_nucleus
    main.cpp
    catch2_helpers.h
    test_Camera.cpp
    nucleus_utils_stopwatch.cpp
    test_DrawListGenerator.cpp
    test_helpers.h
    test_raster.cpp
    test_terrain_mesh_index_generator.cpp
    test_srs.cpp
    test_tile.cpp
    test_tile_conversion.cpp
    nucleus_tile_scheduler_util.cpp
    nucleus_tile_scheduler_tile_load_service.cpp
    nucleus_tile_scheduler_layer_assembler.cpp
    nucleus_tile_scheduler_quad_assembler.cpp
    nucleus_tile_scheduler_cache.cpp
    nucleus_tile_scheduler_scheduler.cpp
    nucleus_tile_scheduler_slot_limiter.cpp
    nucleus_tile_scheduler_rate_limiter.cpp
    RateTester.h RateTester.cpp
    test_zppbits.cpp
    cache_queries.cpp
)

qt_add_resources(unittests_nucleus "height_data"
    PREFIX "/map"
    BASE ${alpine_height_data_SOURCE_DIR}
    FILES ${alpine_height_data_SOURCE_DIR}/height_data.atb
)
qt_add_resources(unittests_nucleus "test_data"
    PREFIX "/test_data"
    BASE ${CMAKE_CURRENT_SOURCE_DIR}/data/
    FILES
    data/170px-Jeune_bouquetin_de_face.jpg
    data/test-tile_ortho.jpeg
    data/test-tile.png
)
target_link_libraries(unittests_nucleus PUBLIC nucleus Catch2::Catch2 Qt::Test)
target_compile_definitions(unittests_nucleus PUBLIC "ALP_TEST_DATA_DIR=\":/test_data/\"")

alp_setup_unittest(unittests_nucleus)

if (ANDROID)
    add_android_openssl_libraries_to(unittests_nucleus)
endif()

if (EMSCRIPTEN)
    # asyncify required for running nested event loops in some of the tests
    target_link_options(unittests_nucleus PUBLIC -sASYNCIFY -Os)
    configure_file(${CMAKE_SOURCE_DIR}/site/unittests.html unittests.html COPYONLY)
    configure_file(${CMAKE_SOURCE_DIR}/site/ansispan.js ansispan.js COPYONLY)
    
    install(FILES 
        "$<TARGET_FILE_DIR:unittests_nucleus>/unittests_nucleus.js"
        "$<TARGET_FILE_DIR:unittests_nucleus>/unittests_nucleus.wasm"
        "$<TARGET_FILE_DIR:unittests_nucleus>/qtloader.js"
        "${CMAKE_SOURCE_DIR}/site/unittests.html"
        "${CMAKE_SOURCE_DIR}/site/ansispan.js"
        "${CMAKE_SOURCE_DIR}/site/mascot.png"
        DESTINATION ${ALP_WWW_INSTALL_DIR})

    if (ALP_ENABLE_THREADING)
        install(FILES "$<TARGET_FILE_DIR:unittests_nucleus>/unittests_nucleus.worker.js" DESTINATION ${ALP_WWW_INSTALL_DIR})
    endif()
endif()
