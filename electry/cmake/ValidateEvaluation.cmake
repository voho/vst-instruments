cmake_minimum_required(VERSION 3.22)

if(NOT DEFINED ELECTRY_EVALUATION_RENDERER
   OR NOT DEFINED ELECTRY_EVALUATION_DIR)
    message(FATAL_ERROR "Evaluation renderer and output directory are required")
endif()

execute_process(
    COMMAND "${ELECTRY_EVALUATION_RENDERER}" "${ELECTRY_EVALUATION_DIR}"
    RESULT_VARIABLE render_result
    OUTPUT_VARIABLE render_output
    ERROR_VARIABLE render_error)
if(NOT render_result EQUAL 0)
    message(FATAL_ERROR "Evaluation render failed: ${render_error}")
endif()

set(manifest_path "${ELECTRY_EVALUATION_DIR}/manifest.json")
file(READ "${manifest_path}" manifest)
string(JSON schema GET "${manifest}" schema)
string(JSON encoding GET "${manifest}" audio_format encoding)
string(JSON channels GET "${manifest}" audio_format channels)
string(JSON bit_depth GET "${manifest}" audio_format bits_per_sample)
string(JSON sample_rate GET "${manifest}" audio_format sample_rate_hz)
string(JSON normalization_type TYPE "${manifest}" audio_format normalization_applied)
string(JSON normalization_applied GET "${manifest}" audio_format normalization_applied)
string(JSON post_render_gain_type TYPE "${manifest}" audio_format post_render_gain)
string(JSON post_render_gain GET "${manifest}" audio_format post_render_gain)
string(JSON protocol_velocity GET "${manifest}" protocol velocity)
string(JSON lead_in_frames GET "${manifest}" protocol lead_in_frames)
string(JSON held_frames GET "${manifest}" protocol held_frames)
string(JSON release_frames GET "${manifest}" protocol release_frames)
string(JSON block_size GET "${manifest}" protocol block_size)
string(JSON target_count LENGTH "${manifest}" protocol targets)
string(JSON probe_count LENGTH "${manifest}" probes)
if(NOT schema STREQUAL "electry-evaluation/v3"
   OR NOT encoding STREQUAL "IEEE_FLOAT"
   OR NOT channels EQUAL 1
   OR NOT bit_depth EQUAL 32
   OR NOT sample_rate EQUAL 44100
   OR NOT normalization_type STREQUAL "BOOLEAN"
   OR normalization_applied
   OR NOT post_render_gain_type STREQUAL "NUMBER"
   OR NOT post_render_gain EQUAL 1
   OR NOT protocol_velocity EQUAL 0.94999999
   OR NOT lead_in_frames EQUAL 11025
   OR NOT held_frames EQUAL 88200
   OR NOT release_frames EQUAL 44100
   OR NOT block_size EQUAL 256
   OR NOT target_count EQUAL 2
   OR NOT probe_count EQUAL 10)
    message(FATAL_ERROR "Evaluation manifest contract changed unexpectedly")
endif()

set(expected_protocol_ids e1 e2)
set(expected_protocol_strings 8 6)
set(expected_protocol_midi_notes 28 40)
set(expected_protocol_frequencies 41.20344461 82.40688923)
foreach(target_index RANGE 0 1)
    list(GET expected_protocol_ids ${target_index} expected_target_id)
    list(GET expected_protocol_strings ${target_index} expected_target_string)
    list(GET expected_protocol_midi_notes ${target_index} expected_target_midi)
    list(GET expected_protocol_frequencies ${target_index} expected_target_frequency)
    string(JSON target_id GET "${manifest}" protocol targets ${target_index} id)
    string(JSON target_string GET "${manifest}" protocol targets ${target_index} string)
    string(JSON target_fret GET "${manifest}" protocol targets ${target_index} fret)
    string(JSON target_midi GET "${manifest}" protocol targets ${target_index} midi_note)
    string(JSON target_frequency GET "${manifest}" protocol targets ${target_index}
        equal_temperament_frequency_hz)
    if(NOT target_id STREQUAL expected_target_id
       OR NOT target_string EQUAL expected_target_string
       OR NOT target_fret EQUAL 0
       OR NOT target_midi EQUAL expected_target_midi
       OR NOT target_frequency EQUAL expected_target_frequency)
        message(FATAL_ERROR
            "Evaluation protocol target ${target_index} changed unexpectedly")
    endif()
endforeach()

set(expected_probe_ids
    e1-open
    e1-palm-mute-light
    e1-palm-mute-medium
    e1-palm-mute-hard
    e1-dead
    e2-open
    e2-palm-mute-light
    e2-palm-mute-medium
    e2-palm-mute-hard
    e2-dead)
set(expected_mute_damping 0.55 0.0 0.55 1.0 0.55
                          0.55 0.0 0.55 1.0 0.55)
set(expected_play_styles sustain palm_mute palm_mute palm_mute dead
                         sustain palm_mute palm_mute palm_mute dead)
set(expected_style_notes 15 16 16 16 21 15 16 16 16 21)
set(expected_target_strings 8 8 8 8 8 6 6 6 6 6)
set(expected_midi_notes 28 28 28 28 28 40 40 40 40 40)
set(expected_frequencies
    41.20344461 41.20344461 41.20344461 41.20344461 41.20344461
    82.40688923 82.40688923 82.40688923 82.40688923 82.40688923)
file(GLOB evaluation_wavs "${ELECTRY_EVALUATION_DIR}/*.wav")
list(LENGTH evaluation_wavs evaluation_wav_count)
if(NOT evaluation_wav_count EQUAL 10)
    message(FATAL_ERROR
        "Evaluation directory has ${evaluation_wav_count} WAVs; expected 10")
endif()

foreach(probe_index RANGE 0 9)
    list(GET expected_probe_ids ${probe_index} expected_probe_id)
    list(GET expected_mute_damping ${probe_index} expected_probe_damping)
    list(GET expected_play_styles ${probe_index} expected_play_style)
    list(GET expected_style_notes ${probe_index} expected_style_note)
    list(GET expected_target_strings ${probe_index} expected_target_string)
    list(GET expected_midi_notes ${probe_index} expected_midi_note)
    list(GET expected_frequencies ${probe_index} expected_frequency)
    string(JSON probe_id GET "${manifest}" probes ${probe_index} id)
    string(JSON file_name GET "${manifest}" probes ${probe_index} file)
    string(JSON play_style GET "${manifest}" probes ${probe_index} play_style)
    string(JSON target_string GET "${manifest}" probes ${probe_index} target_string)
    string(JSON target_fret GET "${manifest}" probes ${probe_index} target_fret)
    string(JSON midi_note GET "${manifest}" probes ${probe_index} midi_note)
    string(JSON frequency GET "${manifest}" probes ${probe_index}
        equal_temperament_frequency_hz)
    string(JSON mute_damping GET "${manifest}" probes ${probe_index} mute_damping)
    string(JSON frame_count GET "${manifest}" probes ${probe_index} frames)
    string(JSON event_count LENGTH "${manifest}" probes ${probe_index} events)
    if(NOT probe_id STREQUAL expected_probe_id
       OR NOT file_name STREQUAL "${expected_probe_id}.wav"
       OR NOT play_style STREQUAL expected_play_style
       OR NOT target_string EQUAL expected_target_string
       OR NOT target_fret EQUAL 0
       OR NOT midi_note EQUAL expected_midi_note
       OR NOT frequency EQUAL expected_frequency
       OR NOT mute_damping EQUAL expected_probe_damping
       OR NOT frame_count EQUAL 143325
       OR NOT event_count EQUAL 4)
        message(FATAL_ERROR
            "Evaluation probe ${probe_index} metadata changed unexpectedly")
    endif()

    string(JSON pick_event_type GET "${manifest}" probes ${probe_index} events 0 type)
    string(JSON pick_event_bank GET "${manifest}" probes ${probe_index} events 0 bank)
    string(JSON pick_event_value GET "${manifest}" probes ${probe_index} events 0 value)
    string(JSON pick_event_note GET "${manifest}" probes ${probe_index} events 0 midi_note)
    string(JSON pick_event_sample GET "${manifest}" probes ${probe_index} events 0 sample)
    string(JSON pick_event_time GET "${manifest}" probes ${probe_index} events 0 time_seconds)

    string(JSON style_event_type GET "${manifest}" probes ${probe_index} events 1 type)
    string(JSON style_event_bank GET "${manifest}" probes ${probe_index} events 1 bank)
    string(JSON style_event_value GET "${manifest}" probes ${probe_index} events 1 value)
    string(JSON style_event_note GET "${manifest}" probes ${probe_index} events 1 midi_note)
    string(JSON style_event_sample GET "${manifest}" probes ${probe_index} events 1 sample)
    string(JSON style_event_time GET "${manifest}" probes ${probe_index} events 1 time_seconds)
    string(JSON note_on_type GET "${manifest}" probes ${probe_index} events 2 type)
    string(JSON note_on_note GET "${manifest}" probes ${probe_index} events 2 midi_note)
    string(JSON note_on_velocity GET "${manifest}" probes ${probe_index} events 2 velocity)
    string(JSON note_on_sample GET "${manifest}" probes ${probe_index} events 2 sample)
    string(JSON note_on_time GET "${manifest}" probes ${probe_index} events 2 time_seconds)

    string(JSON note_off_type GET "${manifest}" probes ${probe_index} events 3 type)
    string(JSON note_off_note GET "${manifest}" probes ${probe_index} events 3 midi_note)
    string(JSON note_off_sample GET "${manifest}" probes ${probe_index} events 3 sample)
    string(JSON note_off_time GET "${manifest}" probes ${probe_index} events 3 time_seconds)
    if(NOT pick_event_type STREQUAL "keyswitch"
       OR NOT pick_event_bank STREQUAL "pick_style"
       OR NOT pick_event_value STREQUAL "down"
       OR NOT pick_event_note EQUAL 12
       OR NOT pick_event_sample EQUAL 0
       OR NOT pick_event_time EQUAL 0
       OR NOT style_event_type STREQUAL "keyswitch"
       OR NOT style_event_bank STREQUAL "play_style"
       OR NOT style_event_value STREQUAL expected_play_style
       OR NOT style_event_note EQUAL expected_style_note
       OR NOT style_event_sample EQUAL 0
       OR NOT style_event_time EQUAL 0
       OR NOT note_on_type STREQUAL "note_on"
       OR NOT note_on_note EQUAL expected_midi_note
       OR NOT note_on_velocity EQUAL 0.94999999
       OR NOT note_on_sample EQUAL 11025
       OR NOT note_on_time EQUAL 0.25
       OR NOT note_off_type STREQUAL "note_off"
       OR NOT note_off_note EQUAL expected_midi_note
       OR NOT note_off_sample EQUAL 99225
       OR NOT note_off_time EQUAL 2.25)
        message(FATAL_ERROR
            "Evaluation probe ${probe_index} event contract changed unexpectedly")
    endif()

    set(wav_path "${ELECTRY_EVALUATION_DIR}/${file_name}")
    file(SIZE "${wav_path}" wav_size)
    if(wav_size LESS 44)
        message(FATAL_ERROR "${file_name} is shorter than a WAV header")
    endif()
    file(READ "${wav_path}" wav_header OFFSET 0 LIMIT 44 HEX)
    string(TOLOWER "${wav_header}" wav_header)
    string(SUBSTRING "${wav_header}" 0 8 riff_tag)
    string(SUBSTRING "${wav_header}" 8 8 riff_size_bytes)
    string(SUBSTRING "${wav_header}" 16 8 wave_tag)
    string(SUBSTRING "${wav_header}" 24 8 fmt_tag)
    string(SUBSTRING "${wav_header}" 32 8 fmt_size_bytes)
    string(SUBSTRING "${wav_header}" 40 8 format_and_channels)
    string(SUBSTRING "${wav_header}" 48 16 sample_and_byte_rates)
    string(SUBSTRING "${wav_header}" 64 4 block_alignment)
    string(SUBSTRING "${wav_header}" 68 4 bit_depth_bytes)
    string(SUBSTRING "${wav_header}" 72 8 data_tag)
    string(SUBSTRING "${wav_header}" 80 8 data_size_bytes)
    if(NOT riff_tag STREQUAL "52494646"
       OR NOT wave_tag STREQUAL "57415645"
       OR NOT fmt_tag STREQUAL "666d7420"
       OR NOT fmt_size_bytes STREQUAL "10000000"
       OR NOT format_and_channels STREQUAL "03000100"
       OR NOT sample_and_byte_rates STREQUAL "44ac000010b10200"
       OR NOT block_alignment STREQUAL "0400"
       OR NOT bit_depth_bytes STREQUAL "2000"
       OR NOT data_tag STREQUAL "64617461")
        message(FATAL_ERROR "${file_name} is not a mono 32-bit float WAV")
    endif()

    set(riff_size 0)
    set(data_size 0)
    foreach(size_field IN ITEMS riff data)
        set(size_bytes "${${size_field}_size_bytes}")
        foreach(byte_index RANGE 0 3)
            math(EXPR byte_offset "${byte_index} * 2")
            string(SUBSTRING "${size_bytes}" ${byte_offset} 2 byte_value)
            math(EXPR ${size_field}_size
                "${${size_field}_size} + (0x${byte_value} << (8 * ${byte_index}))")
        endforeach()
    endforeach()

    math(EXPR expected_data_size "${frame_count} * 4")
    math(EXPR expected_size "44 + ${expected_data_size}")
    math(EXPR expected_riff_size "${expected_size} - 8")
    if(NOT wav_size EQUAL expected_size)
        message(FATAL_ERROR
            "${file_name} has ${wav_size} bytes; expected ${expected_size}")
    endif()
    if(NOT data_size EQUAL expected_data_size
       OR NOT riff_size EQUAL expected_riff_size)
        message(FATAL_ERROR "${file_name} has inconsistent RIFF/data lengths")
    endif()
endforeach()

message(STATUS "${render_output}Evaluation output contract passed")
