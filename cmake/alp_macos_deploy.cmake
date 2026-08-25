include_guard(GLOBAL)

# Bundles the non-Qt shared libraries webgpu_app links against on macOS
# (SDL2, and the vendored radix/glm/stb_slim/ktx dylibs) into
# <bundle>.app/Contents/Frameworks, and points the executable at that
# folder via an @executable_path rpath.
#
# Why this is needed: these libraries are already built with portable,
# @rpath-relative install names/IDs (CMake's default on macOS) - the
# libraries themselves are fine. What's NOT portable is *where* those
# @rpath references get resolved from: CMake's default rpaths on the
# built executable are absolute paths into this build tree / extern/
# (e.g. /Users/you/dev/repos/webigeo/build/.../alp_external/radix/src),
# which won't exist once the .app is copied to another machine, or even
# just moved out of this exact build directory. Copying the actual
# library files into the bundle and adding one rpath that resolves
# relative to the executable's own location fixes that for good.
#
# Qt's own frameworks are handled separately by macdeployqt, which
# understands Qt's plugin/framework layout; this only covers the
# libraries macdeployqt doesn't know about.
function(alp_macos_deploy_bundle target)
    if (NOT APPLE)
        message(FATAL_ERROR "alp_macos_deploy_bundle can only be used on Apple platforms")
    endif()

    find_program(ALP_INSTALL_NAME_TOOL_EXECUTABLE install_name_tool REQUIRED)
    find_program(ALP_CODESIGN_EXECUTABLE codesign REQUIRED)

    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_BUNDLE_CONTENT_DIR:${target}>/Frameworks"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/${ALP_EXTERN_DIR}/sdl/lib/libSDL2-2.0.0.dylib"
            "$<TARGET_FILE:radix>"
            "$<TARGET_FILE:glm>"
            "$<TARGET_FILE:stb_slim>"
            "$<TARGET_FILE:ktx>"
            "$<TARGET_BUNDLE_CONTENT_DIR:${target}>/Frameworks/"
        # idempotent: -add_rpath errors out if the rpath is already present
        # (true on every build after the first), so failure is swallowed here
        COMMAND /bin/sh -c "'${ALP_INSTALL_NAME_TOOL_EXECUTABLE}' -add_rpath @executable_path/../Frameworks '$<TARGET_FILE:${target}>' 2>/dev/null; exit 0"
        # modifying the binary and adding bundle contents invalidates any
        # existing signature; Apple Silicon refuses to launch an unsigned
        # (or invalidly-signed) executable at all, so re-sign ad-hoc
        COMMAND ${ALP_CODESIGN_EXECUTABLE} --force --deep --sign - "$<TARGET_BUNDLE_DIR:${target}>"
        COMMENT "Bundling non-Qt shared libraries into ${target}.app"
        VERBATIM
    )
endfunction()
