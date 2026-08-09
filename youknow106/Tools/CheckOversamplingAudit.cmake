if(NOT DEFINED NORMAL_AUDIT OR NOT DEFINED WORK_AUDIT)
    message(FATAL_ERROR "NORMAL_AUDIT and WORK_AUDIT executables are required")
endif()

execute_process(
    COMMAND "${NORMAL_AUDIT}" --fingerprint
    RESULT_VARIABLE normal_result
    OUTPUT_VARIABLE normal_fingerprint
    ERROR_VARIABLE normal_error)
if(NOT normal_result EQUAL 0)
    message(FATAL_ERROR
        "normal oversampling fingerprint failed (${normal_result}): ${normal_error}")
endif()

execute_process(
    COMMAND "${WORK_AUDIT}" --fingerprint
    RESULT_VARIABLE work_result
    OUTPUT_VARIABLE work_fingerprint
    ERROR_VARIABLE work_error)
if(NOT work_result EQUAL 0)
    message(FATAL_ERROR
        "instrumented oversampling fingerprint failed (${work_result}): ${work_error}")
endif()

if(NOT normal_fingerprint STREQUAL work_fingerprint)
    message(FATAL_ERROR
        "work instrumentation changed the raw-float fingerprint\n"
        "normal:\n${normal_fingerprint}"
        "instrumented:\n${work_fingerprint}")
endif()

execute_process(
    COMMAND "${WORK_AUDIT}" --self-test
    RESULT_VARIABLE counter_result
    OUTPUT_VARIABLE counter_output
    ERROR_VARIABLE counter_error)
if(NOT counter_result EQUAL 0)
    message(FATAL_ERROR
        "oversampling work-counter contract failed (${counter_result}):\n"
        "${counter_output}${counter_error}")
endif()

message(STATUS "oversampling fingerprint parity and work-counter algebra passed")
