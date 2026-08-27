cmake_minimum_required(VERSION 3.22)

if(NOT DEFINED ELECTRY_MUTE_CAPTURE_DIR)
    message(FATAL_ERROR "ELECTRY_MUTE_CAPTURE_DIR is required")
endif()

get_filename_component(capture_dir "${ELECTRY_MUTE_CAPTURE_DIR}" ABSOLUTE)
set(manifest_path "${capture_dir}/manifest.json")
if(NOT EXISTS "${manifest_path}")
    message(FATAL_ERROR "Missing ${manifest_path}")
endif()
file(READ "${manifest_path}" manifest)

function(require_filled value label)
    if("${value}" STREQUAL "" OR "${value}" MATCHES "^REPLACE")
        message(FATAL_ERROR "${label} was not filled in")
    endif()
endfunction()

function(require_json_number json label)
    string(JSON value_type TYPE "${json}" ${ARGN})
    if(NOT value_type STREQUAL "NUMBER")
        message(FATAL_ERROR "${label} must be a JSON Number")
    endif()
endfunction()

function(require_json_string json label)
    string(JSON value_type TYPE "${json}" ${ARGN})
    if(NOT value_type STREQUAL "STRING")
        message(FATAL_ERROR "${label} must be a JSON String")
    endif()
endfunction()

function(require_sha256 value label)
    string(LENGTH "${value}" hash_length)
    if(NOT hash_length EQUAL 64
       OR "${value}" MATCHES "[^0-9A-Fa-f]"
       OR "${value}" MATCHES "^0+$")
        message(FATAL_ERROR "${label} is not a filled SHA-256")
    endif()
endfunction()

function(little_endian_value hex byte_offset byte_count output)
    set(value 0)
    math(EXPR last_byte "${byte_count} - 1")
    foreach(byte_index RANGE 0 ${last_byte})
        math(EXPR character_offset "(${byte_offset} + ${byte_index}) * 2")
        string(SUBSTRING "${hex}" ${character_offset} 2 byte_value)
        math(EXPR value "${value} + (0x${byte_value} << (8 * ${byte_index}))")
    endforeach()
    set(${output} ${value} PARENT_SCOPE)
endfunction()

# DAWs commonly add bext, JUNK, LIST or other RIFF chunks. Walk the chunk
# table instead of assuming the 44-byte canonical header used by Electry's own
# controlled renderer.
function(inspect_float_wav wav_path output_frames)
    file(SIZE "${wav_path}" wav_size)
    if(wav_size LESS 12)
        message(FATAL_ERROR "${wav_path} is shorter than a RIFF header")
    endif()
    file(READ "${wav_path}" riff_header OFFSET 0 LIMIT 12 HEX)
    string(TOLOWER "${riff_header}" riff_header)
    string(SUBSTRING "${riff_header}" 0 8 riff_tag)
    string(SUBSTRING "${riff_header}" 16 8 wave_tag)
    if(NOT riff_tag STREQUAL "52494646" OR NOT wave_tag STREQUAL "57415645")
        message(FATAL_ERROR "${wav_path} is not RIFF/WAVE")
    endif()
    little_endian_value("${riff_header}" 4 4 riff_size)
    math(EXPR riff_end "${riff_size} + 8")
    if(riff_end GREATER wav_size)
        message(FATAL_ERROR "${wav_path} has a truncated RIFF payload")
    endif()
    if(NOT riff_end EQUAL wav_size)
        message(FATAL_ERROR "${wav_path} has bytes outside its RIFF payload")
    endif()

    set(chunk_offset 12)
    set(found_format FALSE)
    set(found_data FALSE)
    while(chunk_offset LESS riff_end)
        math(EXPR header_end "${chunk_offset} + 8")
        if(header_end GREATER riff_end)
            message(FATAL_ERROR "${wav_path} has a truncated chunk header")
        endif()
        file(READ "${wav_path}" chunk_header
             OFFSET ${chunk_offset} LIMIT 8 HEX)
        string(TOLOWER "${chunk_header}" chunk_header)
        string(SUBSTRING "${chunk_header}" 0 8 chunk_id)
        little_endian_value("${chunk_header}" 4 4 chunk_size)
        math(EXPR chunk_data_offset "${chunk_offset} + 8")
        math(EXPR chunk_data_end "${chunk_data_offset} + ${chunk_size}")
        if(chunk_data_end GREATER riff_end)
            message(FATAL_ERROR "${wav_path} has a truncated RIFF chunk")
        endif()

        if(chunk_id STREQUAL "666d7420")
            if(found_format OR chunk_size LESS 16)
                message(FATAL_ERROR "${wav_path} has an invalid fmt chunk")
            endif()
            file(READ "${wav_path}" format_data
                 OFFSET ${chunk_data_offset} LIMIT 16 HEX)
            little_endian_value("${format_data}" 0 2 format_tag)
            little_endian_value("${format_data}" 2 2 channels)
            little_endian_value("${format_data}" 4 4 sample_rate)
            little_endian_value("${format_data}" 8 4 byte_rate)
            little_endian_value("${format_data}" 12 2 block_alignment)
            little_endian_value("${format_data}" 14 2 bits_per_sample)
            set(ieee_float_format FALSE)
            if(format_tag EQUAL 3)
                set(ieee_float_format TRUE)
            elseif(format_tag EQUAL 65534)
                # WAVE_FORMAT_EXTENSIBLE is a normal DAW export choice even
                # for mono float. Accept only its exact IEEE-float subtype,
                # not an arbitrary extensible format wearing a 32-bit header.
                if(chunk_size LESS 40)
                    message(FATAL_ERROR
                        "${wav_path} has a short WAVE_FORMAT_EXTENSIBLE fmt chunk")
                endif()
                file(READ "${wav_path}" extensible_format_data
                     OFFSET ${chunk_data_offset} LIMIT 40 HEX)
                string(TOLOWER "${extensible_format_data}" extensible_format_data)
                little_endian_value("${extensible_format_data}" 16 2 extension_size)
                little_endian_value("${extensible_format_data}" 18 2 valid_bits)
                math(EXPR declared_format_size "18 + ${extension_size}")
                string(SUBSTRING "${extensible_format_data}" 48 32 subtype_guid)
                if(extension_size LESS 22
                   OR declared_format_size GREATER chunk_size
                   OR NOT valid_bits EQUAL 32
                   OR NOT subtype_guid STREQUAL
                       "0300000000001000800000aa00389b71")
                    message(FATAL_ERROR
                        "${wav_path} is not extensible 32-bit IEEE float")
                endif()
                set(ieee_float_format TRUE)
            endif()
            if(NOT ieee_float_format
               OR NOT channels EQUAL 1
               OR NOT sample_rate EQUAL 44100
               OR NOT byte_rate EQUAL 176400
               OR NOT block_alignment EQUAL 4
               OR NOT bits_per_sample EQUAL 32)
                message(FATAL_ERROR
                    "${wav_path} must be mono 44.1-kHz 32-bit IEEE-float WAV")
            endif()
            set(found_format TRUE)
        elseif(chunk_id STREQUAL "64617461")
            if(found_data OR NOT chunk_size GREATER 0)
                message(FATAL_ERROR "${wav_path} has an invalid data chunk")
            endif()
            math(EXPR data_remainder "${chunk_size} % 4")
            if(NOT data_remainder EQUAL 0)
                message(FATAL_ERROR "${wav_path} has a partial float frame")
            endif()
            math(EXPR frame_count "${chunk_size} / 4")
            set(found_data TRUE)
        endif()

        math(EXPR chunk_offset
            "${chunk_data_end} + (${chunk_size} % 2)")
        if(chunk_offset GREATER riff_end)
            message(FATAL_ERROR "${wav_path} is missing a RIFF pad byte")
        endif()
    endwhile()

    if(NOT found_format OR NOT found_data)
        message(FATAL_ERROR "${wav_path} is missing fmt or data")
    endif()
    set(${output_frames} ${frame_count} PARENT_SCOPE)
endfunction()

string(JSON schema GET "${manifest}" schema)
string(JSON split GET "${manifest}" session split)
string(JSON session_id GET "${manifest}" session session_id)
string(JSON player_id GET "${manifest}" session player_id)
string(JSON recorded_utc_type TYPE "${manifest}" session recorded_utc)
string(JSON recorded_utc GET "${manifest}" session recorded_utc)
string(JSON agreement_id GET "${manifest}" session rights agreement_id)
string(JSON agreement_sha GET "${manifest}" session rights agreement_sha256)
string(JSON calibration_right_type TYPE "${manifest}" session rights
    commercial_model_calibration)
string(JSON calibration_right GET "${manifest}" session rights
    commercial_model_calibration)
string(JSON evaluation_right_type TYPE "${manifest}" session rights
    private_competitive_evaluation)
string(JSON evaluation_right GET "${manifest}" session rights
    private_competitive_evaluation)
string(JSON redistribution_type TYPE "${manifest}" session rights
    redistribution_allowed)

if(NOT schema STREQUAL "electry-mute-capture/v1")
    message(FATAL_ERROR "Unexpected capture schema: ${schema}")
endif()
if(NOT split STREQUAL "train" AND NOT split STREQUAL "holdout")
    message(FATAL_ERROR "session.split must be train or holdout")
endif()
require_filled("${session_id}" "session.session_id")
require_filled("${player_id}" "session.player_id")
require_filled("${recorded_utc}" "session.recorded_utc")
require_json_string("${manifest}" "session.session_id" session session_id)
require_json_string("${manifest}" "session.player_id" session player_id)
if(NOT recorded_utc_type STREQUAL "STRING"
   OR NOT recorded_utc MATCHES
      "^[0-9][0-9][0-9][0-9]-(0[1-9]|1[0-2])-(0[1-9]|[12][0-9]|3[01])T([01][0-9]|2[0-3]):[0-5][0-9]:[0-5][0-9]Z$")
    message(FATAL_ERROR
        "session.recorded_utc must be an ISO-8601 UTC timestamp")
endif()
require_filled("${agreement_id}" "session.rights.agreement_id")
require_json_string("${manifest}" "session.rights.agreement_id"
    session rights agreement_id)
require_json_string("${manifest}" "session.rights.agreement_sha256"
    session rights agreement_sha256)
require_sha256("${agreement_sha}" "session.rights.agreement_sha256")
if(NOT calibration_right_type STREQUAL "BOOLEAN"
   OR NOT evaluation_right_type STREQUAL "BOOLEAN"
   OR NOT redistribution_type STREQUAL "BOOLEAN")
    message(FATAL_ERROR "Every rights grant/status must be a JSON Boolean")
endif()
if(NOT calibration_right OR NOT evaluation_right)
    message(FATAL_ERROR
        "Commercial calibration/private evaluation rights must be granted")
endif()

string(JSON container GET "${manifest}" audio_format container)
string(JSON encoding GET "${manifest}" audio_format encoding)
string(JSON bit_depth GET "${manifest}" audio_format bits_per_sample)
string(JSON channels GET "${manifest}" audio_format channels)
string(JSON sample_rate GET "${manifest}" audio_format sample_rate_hz)
string(JSON normalization_type TYPE "${manifest}"
    audio_format normalization_applied)
string(JSON normalization GET "${manifest}" audio_format normalization_applied)
string(JSON processing GET "${manifest}" audio_format processing)
foreach(field IN ITEMS bits_per_sample channels sample_rate_hz)
    require_json_number("${manifest}" "audio_format.${field}"
        audio_format ${field})
endforeach()
if(NOT container STREQUAL "WAVE"
   OR NOT encoding STREQUAL "IEEE_FLOAT"
   OR NOT bit_depth EQUAL 32
   OR NOT channels EQUAL 1
   OR NOT sample_rate EQUAL 44100
   OR NOT normalization_type STREQUAL "BOOLEAN"
   OR normalization
   OR NOT processing STREQUAL "none")
    message(FATAL_ERROR "audio_format does not match the capture contract")
endif()

foreach(field IN ITEMS guitar_id make_model pickup_make_model pickup_mode)
    string(JSON value GET "${manifest}" instrument ${field})
    require_filled("${value}" "instrument.${field}")
    require_json_string("${manifest}" "instrument.${field}"
        instrument ${field})
endforeach()
string(JSON pickup_position GET "${manifest}" instrument pickup_position)
string(JSON scale_length GET "${manifest}" instrument scale_length_inches)
string(JSON pickup_sensing_landmark GET "${manifest}" instrument
    bridge_pickup_sensing_centre_distance_mm landmark)
string(JSON pickup_gap_landmark GET "${manifest}" instrument
    bridge_pickup_string_gap_mm landmark)
string(JSON volume_type TYPE "${manifest}" instrument volume_percent)
string(JSON tone_type TYPE "${manifest}" instrument tone_percent)
string(JSON volume_percent GET "${manifest}" instrument volume_percent)
string(JSON tone_percent GET "${manifest}" instrument tone_percent)
require_json_number("${manifest}" "instrument.scale_length_inches"
    instrument scale_length_inches)
if(NOT pickup_position STREQUAL "bridge"
   OR NOT scale_length GREATER 0
   OR NOT volume_type STREQUAL "NUMBER"
   OR NOT tone_type STREQUAL "NUMBER"
   OR NOT volume_percent EQUAL 100
   OR NOT tone_percent EQUAL 100)
    message(FATAL_ERROR "Instrument bridge pickup/scale metadata is invalid")
endif()
foreach(target IN ITEMS e1 e2)
    string(JSON pickup_sensing_${target}_type TYPE "${manifest}" instrument
        bridge_pickup_sensing_centre_distance_mm ${target})
    string(JSON pickup_sensing_${target} GET "${manifest}" instrument
        bridge_pickup_sensing_centre_distance_mm ${target})
    string(JSON pickup_gap_${target}_type TYPE "${manifest}" instrument
        bridge_pickup_string_gap_mm ${target})
    string(JSON pickup_gap_${target} GET "${manifest}" instrument
        bridge_pickup_string_gap_mm ${target})
    if(NOT pickup_sensing_${target}_type STREQUAL "NUMBER"
       OR NOT pickup_sensing_${target} GREATER 0
       OR NOT pickup_gap_${target}_type STREQUAL "NUMBER"
       OR NOT pickup_gap_${target} GREATER 0)
        message(FATAL_ERROR
            "Bridge-pickup sensing distance/gap for ${target} must be positive")
    endif()
endforeach()
if(NOT pickup_sensing_landmark STREQUAL
       "saddle witness point to bridge-pickup sensing centre, measured along the resting string"
   OR NOT pickup_gap_landmark STREQUAL
       "resting open-string underside to pickup pole or blade top at the sensing centre")
    message(FATAL_ERROR "Bridge-pickup measurement landmarks changed unexpectedly")
endif()

set(expected_tuning E1 B1 E2 A2 D3 G3 B3 E4)
string(JSON tuning_count LENGTH "${manifest}" instrument tuning_low_to_high)
string(JSON gauge_count LENGTH "${manifest}"
    instrument string_gauges_inches_low_to_high)
if(NOT tuning_count EQUAL 8 OR NOT gauge_count EQUAL 8)
    message(FATAL_ERROR "Tuning and string gauges must each contain 8 values")
endif()
foreach(index RANGE 0 7)
    list(GET expected_tuning ${index} expected_note)
    string(JSON actual_note GET "${manifest}"
        instrument tuning_low_to_high ${index})
    string(JSON gauge GET "${manifest}"
        instrument string_gauges_inches_low_to_high ${index})
    require_json_number("${manifest}"
        "instrument.string_gauges_inches_low_to_high[${index}]"
        instrument string_gauges_inches_low_to_high ${index})
    if(NOT actual_note STREQUAL expected_note OR NOT gauge GREATER 0)
        message(FATAL_ERROR "Invalid tuning/gauge at string index ${index}")
    endif()
endforeach()

string(JSON pick_description GET "${manifest}" performance pick_make_material)
string(JSON pick_thickness GET "${manifest}" performance pick_thickness_mm)
require_filled("${pick_description}" "performance.pick_make_material")
require_json_string("${manifest}" "performance.pick_make_material"
    performance pick_make_material)
require_json_number("${manifest}" "performance.pick_thickness_mm"
    performance pick_thickness_mm)
if(NOT pick_thickness GREATER 0)
    message(FATAL_ERROR "performance.pick_thickness_mm must be positive")
endif()
string(JSON pick_contact_landmark GET "${manifest}"
    performance pick_contact_distance_mm landmark)
foreach(target IN ITEMS e1 e2)
    string(JSON pick_contact_${target}_type TYPE "${manifest}"
        performance pick_contact_distance_mm ${target})
    string(JSON pick_contact_${target} GET "${manifest}"
        performance pick_contact_distance_mm ${target})
    if(NOT pick_contact_${target}_type STREQUAL "NUMBER"
       OR NOT pick_contact_${target} GREATER 0)
        message(FATAL_ERROR "Pick-contact ${target} distance must be positive")
    endif()
endforeach()
string(JSON palm_landmark GET "${manifest}"
    performance palm_heel_distance_mm landmark)
string(JSON palm_span_landmark GET "${manifest}"
    performance palm_contact_span_along_string_mm landmark)
string(JSON palm_orientation GET "${manifest}"
    performance palm_hand_orientation)
require_filled("${palm_orientation}" "performance.palm_hand_orientation")
require_json_string("${manifest}" "performance.palm_hand_orientation"
    performance palm_hand_orientation)
string(JSON dead_definition GET "${manifest}" performance dead_definition)
string(JSON dead_contact_landmark GET "${manifest}"
    performance dead_contact landmark)
string(JSON dead_contact_distance_type TYPE "${manifest}"
    performance dead_contact index_pad_centre_nut_distance_mm)
string(JSON dead_contact_distance GET "${manifest}"
    performance dead_contact index_pad_centre_nut_distance_mm)
string(JSON dead_contact_shape GET "${manifest}"
    performance dead_contact contact_span_hand_shape)
require_filled("${dead_contact_shape}"
    "performance.dead_contact.contact_span_hand_shape")
require_json_string("${manifest}"
    "performance.dead_contact.contact_span_hand_shape"
    performance dead_contact contact_span_hand_shape)
string(JSON recording_order_method GET "${manifest}"
    performance recording_order method)
string(JSON recording_order_id GET "${manifest}"
    performance recording_order id)
string(JSON recording_order_count LENGTH "${manifest}"
    performance recording_order files)
foreach(target IN ITEMS e1 e2)
    foreach(position IN ITEMS near middle far)
        string(JSON palm_${target}_${position} GET "${manifest}"
            performance palm_heel_distance_mm ${target} ${position})
        require_json_number("${manifest}"
            "performance.palm_heel_distance_mm.${target}.${position}"
            performance palm_heel_distance_mm ${target} ${position})
        if(NOT palm_${target}_${position} GREATER 0)
            message(FATAL_ERROR
                "Palm ${target} ${position} distance must be positive")
        endif()
        string(JSON palm_span_${target}_${position}_type TYPE "${manifest}"
            performance palm_contact_span_along_string_mm ${target} ${position})
        string(JSON palm_span_${target}_${position} GET "${manifest}"
            performance palm_contact_span_along_string_mm ${target} ${position})
        if(NOT palm_span_${target}_${position}_type STREQUAL "NUMBER"
           OR NOT palm_span_${target}_${position} GREATER 0)
            message(FATAL_ERROR
                "Palm ${target} ${position} contact span must be positive")
        endif()
    endforeach()
    if(NOT palm_${target}_near LESS palm_${target}_middle
       OR NOT palm_${target}_middle LESS palm_${target}_far)
        message(FATAL_ERROR
            "Palm ${target} distances must increase near < middle < far")
    endif()
endforeach()
if(NOT pick_contact_landmark STREQUAL
       "pick-string contact to saddle witness point, measured along the string"
   OR NOT palm_landmark STREQUAL
       "heel centre to saddle witness point, measured along the string"
   OR NOT palm_span_landmark STREQUAL
       "heel contact-footprint length along the string, estimated at playing pressure from the setup photo"
   OR NOT dead_definition STREQUAL
       "Fretting hand lies lightly across all strings without pressing a fret; picking hand clears bridge and strings except for the pick."
   OR NOT dead_contact_landmark STREQUAL
       "nut witness point to index-finger pad centre, measured along string 8"
   OR NOT dead_contact_distance_type STREQUAL "NUMBER"
   OR NOT dead_contact_distance GREATER 0)
    message(FATAL_ERROR
        "Pick/Palm/Dead landmark or Dead definition changed unexpectedly")
endif()

string(JSON isolated_slots GET "${manifest}" performance isolated_protocol slots)
string(JSON isolated_lead GET "${manifest}"
    performance isolated_protocol lead_in_frames)
string(JSON isolated_held GET "${manifest}"
    performance isolated_protocol held_frames)
string(JSON isolated_reset GET "${manifest}"
    performance isolated_protocol reset_frames)
string(JSON isolated_total GET "${manifest}"
    performance isolated_protocol total_frames)
string(JSON isolated_pick_pattern GET "${manifest}"
    performance isolated_protocol pick_pattern)
string(JSON isolated_stroke_count LENGTH "${manifest}"
    performance isolated_protocol slot_strokes)
string(JSON rapid_count_in GET "${manifest}"
    performance rapid_protocol count_in_beats)
string(JSON rapid_count_in_recorded_type TYPE "${manifest}"
    performance rapid_protocol count_in_recorded)
string(JSON rapid_count_in_recorded GET "${manifest}"
    performance rapid_protocol count_in_recorded)
string(JSON rapid_lead_in GET "${manifest}"
    performance rapid_protocol lead_in_min_frames)
string(JSON rapid_hits GET "${manifest}" performance rapid_protocol hits_per_run)
string(JSON rapid_first_stroke GET "${manifest}"
    performance rapid_protocol first_stroke)
string(JSON rapid_pick_pattern GET "${manifest}"
    performance rapid_protocol pick_pattern)
string(JSON rapid_bpms LENGTH "${manifest}"
    performance rapid_protocol allowed_run_bpms)
string(JSON rapid_tempo_order_method GET "${manifest}"
    performance rapid_protocol tempo_order_method)
string(JSON rapid_tempo_order_id GET "${manifest}"
    performance rapid_protocol tempo_order_id)
string(JSON rapid_inter_run_silence GET "${manifest}"
    performance rapid_protocol inter_run_silence_min_frames)
string(JSON rapid_tail_silence GET "${manifest}"
    performance rapid_protocol tail_silence_min_frames)
string(JSON rapid_align_onsets_type TYPE "${manifest}"
    performance rapid_protocol align_onsets_from_audio)
string(JSON rapid_align_onsets GET "${manifest}"
    performance rapid_protocol align_onsets_from_audio)
string(JSON groove_count_in GET "${manifest}"
    performance dead_groove_protocol count_in_beats)
string(JSON groove_count_in_recorded_type TYPE "${manifest}"
    performance dead_groove_protocol count_in_recorded)
string(JSON groove_count_in_recorded GET "${manifest}"
    performance dead_groove_protocol count_in_recorded)
string(JSON groove_lead_in GET "${manifest}"
    performance dead_groove_protocol lead_in_min_frames)
string(JSON groove_runs GET "${manifest}"
    performance dead_groove_protocol runs)
string(JSON groove_bpm GET "${manifest}"
    performance dead_groove_protocol bpm)
string(JSON groove_subdivision GET "${manifest}"
    performance dead_groove_protocol subdivision)
string(JSON groove_ioi GET "${manifest}"
    performance dead_groove_protocol ioi_seconds)
string(JSON groove_first_stroke GET "${manifest}"
    performance dead_groove_protocol first_stroke)
string(JSON groove_pick_pattern GET "${manifest}"
    performance dead_groove_protocol pick_pattern)
string(JSON groove_notes LENGTH "${manifest}"
    performance dead_groove_protocol notes_per_run)
string(JSON groove_inter_run_silence GET "${manifest}"
    performance dead_groove_protocol inter_run_silence_min_frames)
string(JSON groove_tail_silence GET "${manifest}"
    performance dead_groove_protocol tail_silence_min_frames)
string(JSON groove_align_onsets_type TYPE "${manifest}"
    performance dead_groove_protocol align_onsets_from_audio)
string(JSON groove_align_onsets GET "${manifest}"
    performance dead_groove_protocol align_onsets_from_audio)
string(JSON palm_open_count_in GET "${manifest}"
    performance palm_open_groove_protocol count_in_beats)
string(JSON palm_open_count_in_recorded_type TYPE "${manifest}"
    performance palm_open_groove_protocol count_in_recorded)
string(JSON palm_open_count_in_recorded GET "${manifest}"
    performance palm_open_groove_protocol count_in_recorded)
string(JSON palm_open_lead_in GET "${manifest}"
    performance palm_open_groove_protocol lead_in_min_frames)
string(JSON palm_open_runs GET "${manifest}"
    performance palm_open_groove_protocol runs)
string(JSON palm_open_bpm GET "${manifest}"
    performance palm_open_groove_protocol bpm)
string(JSON palm_open_subdivision GET "${manifest}"
    performance palm_open_groove_protocol subdivision)
string(JSON palm_open_ioi GET "${manifest}"
    performance palm_open_groove_protocol ioi_seconds)
string(JSON palm_open_first_stroke GET "${manifest}"
    performance palm_open_groove_protocol first_stroke)
string(JSON palm_open_pick_pattern GET "${manifest}"
    performance palm_open_groove_protocol pick_pattern)
string(JSON palm_open_notes LENGTH "${manifest}"
    performance palm_open_groove_protocol notes_per_run)
string(JSON palm_open_articulations LENGTH "${manifest}"
    performance palm_open_groove_protocol articulations_per_run)
string(JSON palm_open_position GET "${manifest}"
    performance palm_open_groove_protocol palm_e1_position)
string(JSON palm_open_heel_state GET "${manifest}"
    performance palm_open_groove_protocol open_e2_heel_state)
string(JSON palm_open_inter_run_silence GET "${manifest}"
    performance palm_open_groove_protocol inter_run_silence_min_frames)
string(JSON palm_open_tail_silence GET "${manifest}"
    performance palm_open_groove_protocol tail_silence_min_frames)
string(JSON palm_open_align_onsets_type TYPE "${manifest}"
    performance palm_open_groove_protocol align_onsets_from_audio)
string(JSON palm_open_align_onsets GET "${manifest}"
    performance palm_open_groove_protocol align_onsets_from_audio)
foreach(field IN ITEMS slots lead_in_frames held_frames reset_frames total_frames)
    require_json_number("${manifest}"
        "performance.isolated_protocol.${field}"
        performance isolated_protocol ${field})
endforeach()
foreach(field IN ITEMS count_in_beats lead_in_min_frames hits_per_run
                       inter_run_silence_min_frames tail_silence_min_frames)
    require_json_number("${manifest}"
        "performance.rapid_protocol.${field}"
        performance rapid_protocol ${field})
endforeach()
foreach(field IN ITEMS count_in_beats lead_in_min_frames runs bpm ioi_seconds
                       inter_run_silence_min_frames tail_silence_min_frames)
    require_json_number("${manifest}"
        "performance.dead_groove_protocol.${field}"
        performance dead_groove_protocol ${field})
endforeach()
foreach(field IN ITEMS count_in_beats lead_in_min_frames runs bpm ioi_seconds
                       inter_run_silence_min_frames tail_silence_min_frames)
    require_json_number("${manifest}"
        "performance.palm_open_groove_protocol.${field}"
        performance palm_open_groove_protocol ${field})
endforeach()
if(NOT isolated_slots EQUAL 12
   OR NOT isolated_lead EQUAL 11025
   OR NOT isolated_held EQUAL 88200
   OR NOT isolated_reset EQUAL 44100
   OR NOT isolated_total EQUAL 1719900
   OR NOT isolated_pick_pattern STREQUAL "hard_single_by_slot"
   OR NOT isolated_stroke_count EQUAL 12
   OR NOT recording_order_method STREQUAL "randomized_before_recording"
   OR NOT recording_order_count EQUAL 16
   OR NOT rapid_count_in EQUAL 4
   OR NOT rapid_count_in_recorded_type STREQUAL "BOOLEAN"
   OR rapid_count_in_recorded
   OR NOT rapid_lead_in EQUAL 11025
   OR NOT rapid_hits EQUAL 12
   OR NOT rapid_first_stroke STREQUAL "down"
   OR NOT rapid_pick_pattern STREQUAL "hard_alternate"
   OR NOT rapid_bpms EQUAL 3
   OR NOT rapid_tempo_order_method STREQUAL
          "balanced_randomized_before_recording"
   OR NOT rapid_inter_run_silence EQUAL 88200
   OR NOT rapid_tail_silence EQUAL 88200
   OR NOT rapid_align_onsets_type STREQUAL "BOOLEAN"
   OR NOT rapid_align_onsets
   OR NOT groove_count_in EQUAL 4
   OR NOT groove_count_in_recorded_type STREQUAL "BOOLEAN"
   OR groove_count_in_recorded
   OR NOT groove_lead_in EQUAL 11025
   OR NOT groove_runs EQUAL 3
   OR NOT groove_bpm EQUAL 180
   OR NOT groove_subdivision STREQUAL "sixteenth"
   OR NOT groove_ioi EQUAL 0.08333333
   OR NOT groove_first_stroke STREQUAL "down"
   OR NOT groove_pick_pattern STREQUAL "hard_alternate"
   OR NOT groove_notes EQUAL 8
   OR NOT palm_open_count_in EQUAL 4
   OR NOT palm_open_count_in_recorded_type STREQUAL "BOOLEAN"
   OR palm_open_count_in_recorded
   OR NOT palm_open_lead_in EQUAL 11025
   OR NOT palm_open_runs EQUAL 3
   OR NOT palm_open_bpm EQUAL 180
   OR NOT palm_open_subdivision STREQUAL "eighth"
   OR NOT palm_open_ioi EQUAL 0.16666667
   OR NOT palm_open_first_stroke STREQUAL "down"
   OR NOT palm_open_pick_pattern STREQUAL "hard_alternate"
   OR NOT palm_open_notes EQUAL 8
   OR NOT palm_open_articulations EQUAL 8
   OR NOT palm_open_position STREQUAL "middle"
   OR NOT palm_open_heel_state STREQUAL "fully_clear"
   OR NOT palm_open_inter_run_silence EQUAL 88200
   OR NOT palm_open_tail_silence EQUAL 88200
   OR NOT palm_open_align_onsets_type STREQUAL "BOOLEAN"
   OR NOT palm_open_align_onsets)
    message(FATAL_ERROR "Performance protocol changed unexpectedly")
endif()
require_filled("${recording_order_id}" "performance.recording_order.id")
require_filled("${rapid_tempo_order_id}"
    "performance.rapid_protocol.tempo_order_id")
require_json_string("${manifest}" "performance.recording_order.id"
    performance recording_order id)
require_json_string("${manifest}"
    "performance.rapid_protocol.tempo_order_id"
    performance rapid_protocol tempo_order_id)
# Direction-matched isolated references remove the direct down/up mismatch;
# continuous alternate-picking history remains part of the rapid comparison.
foreach(index RANGE 0 11)
    math(EXPR stroke_parity "${index} % 2")
    if(stroke_parity EQUAL 0)
        set(expected_stroke "down")
    else()
        set(expected_stroke "up")
    endif()
    string(JSON actual_stroke GET "${manifest}"
        performance isolated_protocol slot_strokes ${index})
    if(NOT actual_stroke STREQUAL expected_stroke)
        math(EXPR display_slot "${index} + 1")
        message(FATAL_ERROR
            "Isolated slot ${display_slot} must use stroke ${expected_stroke}")
    endif()
endforeach()
if(NOT groove_inter_run_silence EQUAL 88200
   OR NOT groove_tail_silence EQUAL 88200
   OR NOT groove_align_onsets_type STREQUAL "BOOLEAN"
   OR NOT groove_align_onsets)
    message(FATAL_ERROR "Dead groove silence/alignment protocol changed unexpectedly")
endif()
set(expected_bpms 120 180 240)
set(expected_iois 0.125 0.08333333 0.0625)
foreach(index RANGE 0 2)
    list(GET expected_bpms ${index} expected_bpm)
    list(GET expected_iois ${index} expected_ioi)
    string(JSON actual_bpm GET "${manifest}"
        performance rapid_protocol allowed_run_bpms ${index})
    string(JSON actual_ioi GET "${manifest}"
        performance rapid_protocol sixteenth_ioi_seconds_by_bpm ${expected_bpm})
    require_json_number("${manifest}"
        "performance.rapid_protocol.allowed_run_bpms[${index}]"
        performance rapid_protocol allowed_run_bpms ${index})
    require_json_number("${manifest}"
        "performance.rapid_protocol.sixteenth_ioi_seconds_by_bpm.${expected_bpm}"
        performance rapid_protocol sixteenth_ioi_seconds_by_bpm ${expected_bpm})
    if(NOT actual_bpm EQUAL expected_bpm
       OR NOT actual_ioi EQUAL expected_ioi)
        message(FATAL_ERROR "Rapid run order changed unexpectedly")
    endif()
endforeach()
foreach(bpm IN LISTS expected_bpms)
    foreach(position RANGE 0 2)
        set(tempo_${bpm}_position_${position} 0)
    endforeach()
endforeach()
set(expected_groove E1 E1 E1 E2 E1 E1 E2 E1)
foreach(index RANGE 0 7)
    list(GET expected_groove ${index} expected_note)
    string(JSON actual_note GET "${manifest}"
        performance dead_groove_protocol notes_per_run ${index})
    if(NOT actual_note STREQUAL expected_note)
        message(FATAL_ERROR "Dead groove pattern changed unexpectedly")
    endif()
endforeach()
set(expected_palm_open_notes E1 E1 E1 E2 E1 E1 E2 E1)
set(expected_palm_open_articulations palm palm palm open palm palm open palm)
foreach(index RANGE 0 7)
    list(GET expected_palm_open_notes ${index} expected_note)
    list(GET expected_palm_open_articulations ${index}
        expected_articulation)
    string(JSON actual_note GET "${manifest}"
        performance palm_open_groove_protocol notes_per_run ${index})
    string(JSON actual_articulation GET "${manifest}"
        performance palm_open_groove_protocol articulations_per_run ${index})
    if(NOT actual_note STREQUAL expected_note
       OR NOT actual_articulation STREQUAL expected_articulation)
        message(FATAL_ERROR "Palm/Open groove pattern changed unexpectedly")
    endif()
endforeach()

foreach(field IN ITEMS interface_make_model di_or_preamp
                       guitar_output_cable_make_model
                       guitar_output_cable_capacitance_basis notes)
    string(JSON value GET "${manifest}" signal_chain ${field})
    require_filled("${value}" "signal_chain.${field}")
    require_json_string("${manifest}" "signal_chain.${field}"
        signal_chain ${field})
endforeach()
string(JSON input_impedance GET "${manifest}"
    signal_chain input_impedance_ohms)
string(JSON interface_gain_type TYPE "${manifest}"
    signal_chain interface_gain_db)
string(JSON interface_gain GET "${manifest}"
    signal_chain interface_gain_db)
string(JSON cable_length_type TYPE "${manifest}"
    signal_chain guitar_output_cable_length_m)
string(JSON cable_length GET "${manifest}"
    signal_chain guitar_output_cable_length_m)
string(JSON cable_capacitance_type TYPE "${manifest}"
    signal_chain guitar_output_cable_total_capacitance_pf)
string(JSON cable_capacitance GET "${manifest}"
    signal_chain guitar_output_cable_total_capacitance_pf)
require_json_number("${manifest}" "signal_chain.input_impedance_ohms"
    signal_chain input_impedance_ohms)
if(NOT input_impedance GREATER 0
   OR NOT interface_gain_type STREQUAL "NUMBER"
   OR interface_gain LESS -60
   OR interface_gain GREATER 100
   OR NOT cable_length_type STREQUAL "NUMBER"
   OR NOT cable_length GREATER 0
   OR cable_length GREATER 100
   OR NOT cable_capacitance_type STREQUAL "NUMBER"
   OR NOT cable_capacitance GREATER 0
   OR cable_capacitance GREATER 10000)
    message(FATAL_ERROR
        "Signal-chain impedance, gain or guitar-cable metadata is invalid")
endif()

set(expected_files
    e1-open.wav
    e1-palm-near.wav
    e1-palm-middle.wav
    e1-palm-far.wav
    e1-dead.wav
    e2-open.wav
    e2-palm-near.wav
    e2-palm-middle.wav
    e2-palm-far.wav
    e2-dead.wav
    e1-palm-middle-rapid.wav
    e2-palm-middle-rapid.wav
    e1-dead-rapid.wav
    e2-dead-rapid.wav
    dead-e1-e2-groove.wav
    palm-open-e1-e2-groove.wav)
set(expected_kinds
    isolated isolated isolated isolated isolated
    isolated isolated isolated isolated isolated
    rapid rapid rapid rapid groove groove)
set(expected_targets
    e1 e1 e1 e1 e1 e2 e2 e2 e2 e2 e1 e2 e1 e2
    mixed_e1_e2 mixed_e1_e2)
set(expected_articulations
    open palm palm palm dead open palm palm palm dead palm palm dead dead dead
    palm_open_transition)
set(expected_palm_positions
    none near middle far none none near middle far none middle middle none none
    none middle)
set(expected_string_numbers
    8 8 8 8 8 6 6 6 6 6 8 6 8 6 mixed mixed)

set(seen_recording_order)
foreach(index RANGE 0 15)
    string(JSON recorded_file GET "${manifest}"
        performance recording_order files ${index})
    list(FIND expected_files "${recorded_file}" expected_file_index)
    list(FIND seen_recording_order "${recorded_file}" duplicate_order_index)
    if(expected_file_index LESS 0)
        message(FATAL_ERROR
            "Recording order contains unexpected ${recorded_file}")
    endif()
    if(NOT duplicate_order_index EQUAL -1)
        message(FATAL_ERROR
            "Recording order repeats ${recorded_file}")
    endif()
    list(APPEND seen_recording_order "${recorded_file}")
endforeach()

string(JSON take_count LENGTH "${manifest}" takes)
if(NOT take_count EQUAL 16)
    message(FATAL_ERROR "Capture manifest must contain exactly 16 takes")
endif()
file(GLOB capture_wavs LIST_DIRECTORIES false "${capture_dir}/*.wav")
list(LENGTH capture_wavs wav_count)
if(NOT wav_count EQUAL 16)
    message(FATAL_ERROR "Capture directory must contain exactly 16 WAV files")
endif()

set(seen_wav_hashes)
foreach(index RANGE 0 15)
    list(GET expected_files ${index} expected_file)
    list(GET expected_kinds ${index} expected_kind)
    list(GET expected_targets ${index} expected_target)
    list(GET expected_articulations ${index} expected_articulation)
    list(GET expected_palm_positions ${index} expected_palm_position)
    list(GET expected_string_numbers ${index} expected_string_number)
    string(JSON file_name GET "${manifest}" takes ${index} file)
    string(JSON kind GET "${manifest}" takes ${index} kind)
    string(JSON target GET "${manifest}" takes ${index} target)
    string(JSON articulation GET "${manifest}" takes ${index} articulation)
    string(JSON pick_pattern GET "${manifest}" takes ${index} pick_pattern)
    string(JSON declared_frames GET "${manifest}" takes ${index} frames)
    string(JSON declared_hash GET "${manifest}" takes ${index} sha256)
    require_json_number("${manifest}" "takes[${index}].frames"
        takes ${index} frames)
    require_json_string("${manifest}" "takes[${index}].sha256"
        takes ${index} sha256)
    if(NOT file_name STREQUAL expected_file
       OR NOT kind STREQUAL expected_kind
       OR NOT target STREQUAL expected_target
       OR NOT articulation STREQUAL expected_articulation)
        message(FATAL_ERROR "Take ${index} identity changed unexpectedly")
    endif()
    if(expected_string_number STREQUAL "mixed")
        string(JSON string_number_count LENGTH "${manifest}"
            takes ${index} string_numbers)
        string(JSON first_string_number GET "${manifest}"
            takes ${index} string_numbers 0)
        string(JSON second_string_number GET "${manifest}"
            takes ${index} string_numbers 1)
        require_json_number("${manifest}"
            "takes[${index}].string_numbers[0]"
            takes ${index} string_numbers 0)
        require_json_number("${manifest}"
            "takes[${index}].string_numbers[1]"
            takes ${index} string_numbers 1)
        if(NOT string_number_count EQUAL 2
           OR NOT first_string_number EQUAL 8
           OR NOT second_string_number EQUAL 6)
            message(FATAL_ERROR "${file_name} must declare strings 8 and 6")
        endif()
    else()
        string(JSON string_number GET "${manifest}"
            takes ${index} string_number)
        require_json_number("${manifest}" "takes[${index}].string_number"
            takes ${index} string_number)
        if(NOT string_number EQUAL expected_string_number)
            message(FATAL_ERROR "${file_name} has the wrong string number")
        endif()
    endif()
    if(expected_kind STREQUAL "isolated"
       AND NOT pick_pattern STREQUAL "hard_single_by_slot")
        message(FATAL_ERROR
            "${file_name} must follow the isolated slot-stroke schedule")
    elseif(NOT expected_kind STREQUAL "isolated"
           AND NOT pick_pattern STREQUAL "hard_alternate")
        message(FATAL_ERROR "${file_name} must use hard alternate picking")
    endif()
    if(NOT expected_palm_position STREQUAL "none")
        string(JSON palm_position GET "${manifest}"
            takes ${index} palm_position)
        if(NOT palm_position STREQUAL expected_palm_position)
            message(FATAL_ERROR "${file_name} has the wrong Palm position")
        endif()
    endif()

    if(expected_kind STREQUAL "rapid")
        string(JSON run_bpm_count LENGTH "${manifest}"
            takes ${index} run_bpms_in_order)
        if(NOT run_bpm_count EQUAL 3)
            message(FATAL_ERROR
                "${file_name} must declare exactly three run tempos")
        endif()
        set(seen_run_bpms)
        foreach(run_position RANGE 0 2)
            string(JSON run_bpm_type TYPE "${manifest}"
                takes ${index} run_bpms_in_order ${run_position})
            string(JSON run_bpm GET "${manifest}"
                takes ${index} run_bpms_in_order ${run_position})
            list(FIND expected_bpms "${run_bpm}" allowed_bpm_index)
            list(FIND seen_run_bpms "${run_bpm}" duplicate_bpm_index)
            if(NOT run_bpm_type STREQUAL "NUMBER"
               OR allowed_bpm_index LESS 0
               OR NOT duplicate_bpm_index EQUAL -1)
                message(FATAL_ERROR
                    "${file_name} run tempos must permute 120, 180 and 240 BPM")
            endif()
            list(APPEND seen_run_bpms "${run_bpm}")
            set(counter_name "tempo_${run_bpm}_position_${run_position}")
            math(EXPR ${counter_name} "${${counter_name}} + 1")
        endforeach()
    endif()

    if(expected_kind STREQUAL "isolated")
        if(NOT declared_frames EQUAL 1719900)
            message(FATAL_ERROR "${file_name} must contain 1719900 frames")
        endif()
    elseif(expected_kind STREQUAL "rapid"
           AND declared_frames LESS 407007)
        message(FATAL_ERROR
            "${file_name} is too short for three rapid runs and their silences")
    elseif(file_name STREQUAL "palm-open-e1-e2-groove.wav"
           AND declared_frames LESS 429975)
        message(FATAL_ERROR
            "${file_name} is too short for three Palm/Open transition runs and their silences")
    elseif(expected_kind STREQUAL "groove"
           AND declared_frames LESS 352800)
        message(FATAL_ERROR
            "${file_name} is too short for three groove runs and their silences")
    endif()
    require_sha256("${declared_hash}" "${file_name} sha256")

    set(wav_path "${capture_dir}/${file_name}")
    if(NOT EXISTS "${wav_path}")
        message(FATAL_ERROR "Missing ${wav_path}")
    endif()
    inspect_float_wav("${wav_path}" actual_frames)
    if(NOT actual_frames EQUAL declared_frames)
        message(FATAL_ERROR
            "${file_name} has ${actual_frames} frames; manifest says ${declared_frames}")
    endif()
    file(SHA256 "${wav_path}" actual_hash)
    string(TOLOWER "${declared_hash}" declared_hash)
    if(NOT actual_hash STREQUAL declared_hash)
        message(FATAL_ERROR "${file_name} SHA-256 does not match its manifest")
    endif()
    list(FIND seen_wav_hashes "${actual_hash}" duplicate_hash_index)
    if(NOT duplicate_hash_index EQUAL -1)
        message(FATAL_ERROR "${file_name} duplicates an earlier take byte-for-byte")
    endif()
    list(APPEND seen_wav_hashes "${actual_hash}")
endforeach()

foreach(bpm IN LISTS expected_bpms)
    set(min_position_count 99)
    set(max_position_count 0)
    foreach(position RANGE 0 2)
        set(counter_name "tempo_${bpm}_position_${position}")
        set(position_count "${${counter_name}}")
        if(position_count LESS min_position_count)
            set(min_position_count ${position_count})
        endif()
        if(position_count GREATER max_position_count)
            set(max_position_count ${position_count})
        endif()
    endforeach()
    math(EXPR position_spread "${max_position_count} - ${min_position_count}")
    if(position_spread GREATER 1)
        message(FATAL_ERROR
            "Rapid ${bpm} BPM run positions are not balanced across files")
    endif()
endforeach()

message(STATUS
    "Electry mute capture ${session_id} (${split}) passed structural intake")
