include_guard()

function(cntp_target_name targetName targetReadableName)
    IF(BLUEPRINT)
        set(TARGET_READABLE_NAME "${targetReadableName} Blueprint")
    ELSE()
        set(TARGET_READABLE_NAME "${targetReadableName}")
    ENDIF()

    IF(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
        set(TARGET_FINAL_NAME ${TARGET_READABLE_NAME})
    ELSE()
        string(TOLOWER ${targetReadableName} TARGET_LOWER_NAME)
        string(REPLACE " " "-" TARGET_LOWER_SKEWER_NAME ${TARGET_LOWER_NAME})
        IF(BLUEPRINT)
            set(TARGET_FINAL_NAME "${TARGET_LOWER_SKEWER_NAME}-blueprint")
        ELSE()
            set(TARGET_FINAL_NAME "${TARGET_LOWER_SKEWER_NAME}")
        ENDIF()
    ENDIF()


    set_target_properties(${targetName} PROPERTIES
            OUTPUT_NAME "${TARGET_FINAL_NAME}")
    target_compile_definitions(${targetName} PRIVATE
            T_APPMETA_READABLE_NAME="${TARGET_READABLE_NAME}")
endfunction()
