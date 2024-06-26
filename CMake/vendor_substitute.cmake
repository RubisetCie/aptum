file(READ ${IN} input)
foreach(variable ${VARS})
    execute_process(COMMAND ${PROJECT_SOURCE_DIR}/vendor/getinfo
                            --vendor ${CURRENT_VENDOR} ${variable}
                    OUTPUT_VARIABLE value OUTPUT_STRIP_TRAILING_WHITESPACE
                    COMMAND_ERROR_IS_FATAL ANY)
    string(REPLACE "&${variable};" "${value}" input "${input}")
endforeach()
file(WRITE ${OUT} "${input}")
