cmake_minimum_required(VERSION 3.22)

if(NOT DEFINED ELECTRY_MUTE_CAPTURE_COLLECTION_DIR)
    message(FATAL_ERROR "ELECTRY_MUTE_CAPTURE_COLLECTION_DIR is required")
endif()

get_filename_component(collection_dir
    "${ELECTRY_MUTE_CAPTURE_COLLECTION_DIR}" ABSOLUTE)
if(NOT IS_DIRECTORY "${collection_dir}")
    message(FATAL_ERROR "${collection_dir} is not a directory")
endif()

file(GLOB_RECURSE manifests LIST_DIRECTORIES false
    "${collection_dir}/manifest.json")
list(SORT manifests)
if(NOT manifests)
    message(FATAL_ERROR "No manifest.json files found under ${collection_dir}")
endif()

set(session_ids)
set(train_players)
set(train_guitars)
set(holdout_players)
set(holdout_guitars)
set(train_sessions 0)
set(holdout_sessions 0)

foreach(manifest_path IN LISTS manifests)
    get_filename_component(session_dir "${manifest_path}" DIRECTORY)
    execute_process(
        COMMAND "${CMAKE_COMMAND}"
            "-DELECTRY_MUTE_CAPTURE_DIR=${session_dir}"
            -P "${CMAKE_CURRENT_LIST_DIR}/ValidateMuteCapture.cmake"
        RESULT_VARIABLE validation_result
        OUTPUT_VARIABLE validation_output
        ERROR_VARIABLE validation_error)
    if(NOT validation_result EQUAL 0)
        message(FATAL_ERROR
            "Session intake failed for ${session_dir}:\n"
            "${validation_output}${validation_error}")
    endif()

    file(READ "${manifest_path}" manifest)
    string(JSON session_id GET "${manifest}" session session_id)
    string(JSON player_id GET "${manifest}" session player_id)
    string(JSON guitar_id GET "${manifest}" instrument guitar_id)
    string(JSON split GET "${manifest}" session split)
    string(SHA256 session_key "session:${session_id}")
    string(SHA256 player_key "player:${player_id}")
    string(SHA256 guitar_key "guitar:${guitar_id}")

    list(FIND session_ids "${session_key}" duplicate_session)
    if(NOT duplicate_session EQUAL -1)
        message(FATAL_ERROR "Duplicate session.session_id: ${session_id}")
    endif()
    list(APPEND session_ids "${session_key}")
    set(player_label_${player_key} "${player_id}")
    set(guitar_label_${guitar_key} "${guitar_id}")

    if(split STREQUAL "train")
        list(APPEND train_players "${player_key}")
        list(APPEND train_guitars "${guitar_key}")
        math(EXPR train_sessions "${train_sessions} + 1")
    else()
        list(APPEND holdout_players "${player_key}")
        list(APPEND holdout_guitars "${guitar_key}")
        math(EXPR holdout_sessions "${holdout_sessions} + 1")
    endif()
endforeach()

if(train_sessions EQUAL 0 OR holdout_sessions EQUAL 0)
    message(FATAL_ERROR "Collection must contain train and holdout sessions")
endif()

list(REMOVE_DUPLICATES train_players)
list(REMOVE_DUPLICATES train_guitars)
list(REMOVE_DUPLICATES holdout_players)
list(REMOVE_DUPLICATES holdout_guitars)
foreach(player_key IN LISTS train_players)
    list(FIND holdout_players "${player_key}" leaked_player)
    if(NOT leaked_player EQUAL -1)
        message(FATAL_ERROR
            "player_id ${player_label_${player_key}} occurs in train and holdout")
    endif()
endforeach()
foreach(guitar_key IN LISTS train_guitars)
    list(FIND holdout_guitars "${guitar_key}" leaked_guitar)
    if(NOT leaked_guitar EQUAL -1)
        message(FATAL_ERROR
            "guitar_id ${guitar_label_${guitar_key}} occurs in train and holdout")
    endif()
endforeach()

list(LENGTH manifests session_count)
message(STATUS
    "Electry mute collection structural intake passed: ${session_count} sessions "
    "(${train_sessions} train, ${holdout_sessions} holdout), "
    "no declared-ID leakage")
