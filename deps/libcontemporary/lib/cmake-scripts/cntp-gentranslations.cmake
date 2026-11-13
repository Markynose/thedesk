include_guard()

function(cntp_generate_translations target)
    message("-- Finding translation files to generate translations for (target ${target})")

    file(GLOB TRANSLATION_FILES ${CMAKE_CURRENT_SOURCE_DIR}/translations/*.ts)
    list(PREPEND TRANSLATION_FILES ${CMAKE_CURRENT_SOURCE_DIR}/translations/en_US.ts)

    add_custom_target(${target}_CNTPTRANSLATIONS
            COMMAND lupdate . -ts ${TRANSLATION_FILES} -no-obsolete -silent
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            COMMENT "Generating translation files"
            VERBATIM)
    add_dependencies(${target} ${target}_CNTPTRANSLATIONS)
endfunction()
