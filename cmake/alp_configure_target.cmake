function(alp_configure_target target_name)
    if (ANDROID)
        set_target_properties(${target_name} PROPERTIES
            QT_ANDROID_MIN_SDK_VERSION "${ALP_ANDROID_MIN_SDK_VERSION}"
            QT_ANDROID_TARGET_SDK_VERSION "${ALP_ANDROID_TARGET_SDK_VERSION}"
            QT_ANDROID_COMPILE_SDK_VERSION "${ALP_ANDROID_COMPILE_SDK_VERSION}"
        )
    endif()
endfunction()
