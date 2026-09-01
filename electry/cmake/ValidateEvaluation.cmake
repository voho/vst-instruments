cmake_minimum_required(VERSION 3.22)

if(NOT DEFINED ELECTRY_EVALUATION_RENDERER
   OR NOT DEFINED ELECTRY_EVALUATION_DIR)
    message(FATAL_ERROR "Evaluation renderer and output directory are required")
endif()

execute_process(
    COMMAND "${ELECTRY_EVALUATION_RENDERER}" --self-test
            "${ELECTRY_EVALUATION_DIR}"
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

set(metal_pass_a "${ELECTRY_EVALUATION_DIR}/metal-pass-a")
set(metal_pass_b "${ELECTRY_EVALUATION_DIR}/metal-pass-b")
file(REMOVE_RECURSE "${metal_pass_a}" "${metal_pass_b}")
foreach(metal_pass IN ITEMS "${metal_pass_a}" "${metal_pass_b}")
    execute_process(
        COMMAND "${ELECTRY_EVALUATION_RENDERER}" --metal-benchmark
                "${metal_pass}"
        RESULT_VARIABLE metal_render_result
        OUTPUT_VARIABLE metal_render_output
        ERROR_VARIABLE metal_render_error)
    if(NOT metal_render_result EQUAL 0)
        message(FATAL_ERROR "Metal benchmark render failed: ${metal_render_error}")
    endif()
endforeach()

set(metal_manifest_path "${metal_pass_a}/manifest.json")
file(READ "${metal_manifest_path}" metal_manifest)
string(JSON metal_schema GET "${metal_manifest}" schema)
foreach(metal_feature IN ITEMS
        analytic_release_ic
        decoupled_pick_release
        measured_body_response
        low_string_loss_correction_order2
        energy_attack_pitch
        positioned_fret_collision
        measured_pickup_flux
        passive_repick_spring
        measured_modern_cabinet)
    string(JSON metal_feature_type TYPE "${metal_manifest}"
        build_features ${metal_feature})
    string(JSON metal_feature_value GET "${metal_manifest}"
        build_features ${metal_feature})
    string(TOUPPER "${metal_feature}" metal_feature_upper)
    set(metal_expected_feature_variable
        "ELECTRY_EXPECT_${metal_feature_upper}")
    if(NOT metal_feature_type STREQUAL "BOOLEAN")
        message(FATAL_ERROR
            "Metal benchmark build feature ${metal_feature} is not Boolean")
    endif()
    if(NOT DEFINED ${metal_expected_feature_variable}
       OR NOT "${metal_feature_value}" STREQUAL
              "${${metal_expected_feature_variable}}")
        message(FATAL_ERROR
            "Metal benchmark build feature ${metal_feature} disagrees with its target")
    endif()
endforeach()
string(JSON metal_encoding GET "${metal_manifest}" audio_format encoding)
string(JSON metal_channels GET "${metal_manifest}" audio_format channels)
string(JSON metal_bit_depth GET "${metal_manifest}" audio_format bits_per_sample)
string(JSON metal_sample_rate GET "${metal_manifest}" audio_format sample_rate_hz)
string(JSON metal_normalization_type TYPE "${metal_manifest}"
    audio_format normalization_applied)
string(JSON metal_normalization GET "${metal_manifest}"
    audio_format normalization_applied)
string(JSON metal_block_size GET "${metal_manifest}" signal_chain outer_block_size)
string(JSON metal_feedback_delay GET "${metal_manifest}"
    signal_chain feedback_chunk_limit_samples)
string(JSON metal_feedback_transport_type TYPE "${metal_manifest}"
    signal_chain feedback_transport_active)
string(JSON metal_feedback_transport GET "${metal_manifest}"
    signal_chain feedback_transport_active)
string(JSON metal_feedback_injection_type TYPE "${metal_manifest}"
    signal_chain acoustic_feedback_injection_active)
string(JSON metal_feedback_injection GET "${metal_manifest}"
    signal_chain acoustic_feedback_injection_active)
string(JSON metal_acoustic_level GET "${metal_manifest}"
    signal_chain acoustic_return_level)
string(JSON metal_score_id GET "${metal_manifest}" protocol score_id)
string(JSON metal_score_randomization_type TYPE "${metal_manifest}"
    protocol score_randomization)
string(JSON metal_score_randomization GET "${metal_manifest}"
    protocol score_randomization)
string(JSON metal_variation_seed GET "${metal_manifest}" protocol variation_seed)
string(JSON metal_target_count LENGTH "${metal_manifest}" protocol targets)
string(JSON metal_lead_in GET "${metal_manifest}" protocol lead_in_frames)
string(JSON metal_pre_score_wait GET "${metal_manifest}" protocol
    pre_score_wait_frames)
string(JSON metal_hit_hold GET "${metal_manifest}" protocol hit_hold_frames)
string(JSON metal_hit_gap GET "${metal_manifest}" protocol hit_gap_frames)
string(JSON metal_phrase_gap GET "${metal_manifest}" protocol
    inter_phrase_gap_frames)
string(JSON metal_tail GET "${metal_manifest}" protocol tail_frames)
string(JSON metal_hit_count GET "${metal_manifest}" protocol hit_count)
string(JSON metal_total_frames GET "${metal_manifest}" protocol total_frames)
string(JSON metal_pick_style GET "${metal_manifest}" protocol pick_style)
string(JSON metal_pick_keyswitch GET "${metal_manifest}" protocol
    pick_keyswitch_midi_note)
string(JSON metal_keyswitch_velocity GET "${metal_manifest}" protocol
    keyswitch_velocity)
string(JSON metal_velocity_count LENGTH "${metal_manifest}" protocol
    velocity_pattern)
string(JSON metal_velocity_accent GET "${metal_manifest}" protocol
    velocity_pattern 0)
string(JSON metal_velocity_secondary GET "${metal_manifest}" protocol
    velocity_pattern 1)
string(JSON metal_phrase_count LENGTH "${metal_manifest}" protocol phrases)
string(JSON metal_pickup_selector GET "${metal_manifest}" engine_parameters
    pickup_selector)
string(JSON metal_output_mode GET "${metal_manifest}" engine_parameters
    output_mode)
string(JSON metal_amp_model GET "${metal_manifest}" fx_parameters amp_model)
string(JSON metal_distortion GET "${metal_manifest}" fx_parameters distortion)
string(JSON metal_amp GET "${metal_manifest}" fx_parameters amp)
string(JSON metal_compressor GET "${metal_manifest}" fx_parameters compressor)
string(JSON metal_delay GET "${metal_manifest}" fx_parameters delay)
string(JSON metal_room GET "${metal_manifest}" fx_parameters room)
string(JSON metal_output_count LENGTH "${metal_manifest}" outputs)

# These are part of the benchmark stimulus, not descriptive metadata. Keep the
# complete numerical contract here so a renderer edit cannot silently change a
# guitar/control value while two same-build passes remain byte-identical.
set(metal_engine_numeric_contract
    guitar_build=0.80000001
    body_wood=0.80000001
    body_size=0.50000000
    body_shape=0.50000000
    construction=0.50000000
    scale_length=0.85000002
    pickup_type=0.31999999
    tone_knob=1.00000000
    body_resonance=0.34999999
    string_gauge=1.00000000
    string_age=0.10000000
    pick_position=0.18000001
    pick_hardness=0.85000002
    pick_noise=0.50000000
    finger_noise=0.55000001
    release_noise=0.40000001
    mute_damping=0.85000002
    bend_time_seconds=0.28000000
    velocity_amount=0.69999999
    output_gain=2.00000000
    artifact_amount=0.15000001
    sympathetic_amount=0.25000000
    palm_mute=0.00000000
    strum_spread_seconds=0.00000000
    tremolo_rate_hz=12.00000000
    resonance_depth=0.34999999
    vibrato_depth=0.30000001)
foreach(metal_engine_pair IN LISTS metal_engine_numeric_contract)
    string(REPLACE "=" ";" metal_engine_parts "${metal_engine_pair}")
    list(GET metal_engine_parts 0 metal_engine_key)
    list(GET metal_engine_parts 1 metal_engine_expected)
    string(JSON metal_engine_actual GET "${metal_manifest}"
        engine_parameters "${metal_engine_key}")
    if(NOT metal_engine_actual EQUAL metal_engine_expected)
        message(FATAL_ERROR
            "Metal benchmark engine parameter ${metal_engine_key} changed")
    endif()
endforeach()

set(metal_performance_numeric_contract
    pitch_bend=0.00000000
    mod_wheel_resonance=0.00000000
    palm_mute_pressure=0.00000000
    fretting_vibrato=0.00000000)
foreach(metal_control_pair IN LISTS metal_performance_numeric_contract)
    string(REPLACE "=" ";" metal_control_parts "${metal_control_pair}")
    list(GET metal_control_parts 0 metal_control_key)
    list(GET metal_control_parts 1 metal_control_expected)
    string(JSON metal_control_actual GET "${metal_manifest}"
        performance_controls "${metal_control_key}")
    if(NOT metal_control_actual EQUAL metal_control_expected)
        message(FATAL_ERROR
            "Metal benchmark performance control ${metal_control_key} changed")
    endif()
endforeach()

foreach(metal_control_key IN ITEMS tremolo_picking sustain_pedal)
    string(JSON metal_control_type TYPE "${metal_manifest}"
        performance_controls "${metal_control_key}")
    string(JSON metal_control_actual GET "${metal_manifest}"
        performance_controls "${metal_control_key}")
    if(NOT metal_control_type STREQUAL "BOOLEAN" OR metal_control_actual)
        message(FATAL_ERROR
            "Metal benchmark performance control ${metal_control_key} changed")
    endif()
endforeach()

if(NOT metal_schema STREQUAL "electry-metal-benchmark/v1"
   OR NOT metal_encoding STREQUAL "IEEE_FLOAT"
   OR NOT metal_channels EQUAL 1
   OR NOT metal_bit_depth EQUAL 32
   OR NOT metal_sample_rate EQUAL 44100
   OR NOT metal_normalization_type STREQUAL "BOOLEAN"
   OR metal_normalization
   OR NOT metal_block_size EQUAL 256
   OR metal_feedback_delay LESS 1
   OR NOT metal_feedback_transport_type STREQUAL "BOOLEAN"
   OR NOT metal_feedback_transport
   OR NOT metal_feedback_injection_type STREQUAL "BOOLEAN"
   OR metal_feedback_injection
   OR NOT metal_acoustic_level EQUAL 1
   OR NOT metal_score_id STREQUAL "mute-and-dead-metal/e1-e2/v1"
   OR NOT metal_score_randomization_type STREQUAL "BOOLEAN"
   OR metal_score_randomization
   OR NOT metal_variation_seed EQUAL 0
   OR NOT metal_target_count EQUAL 2
   OR NOT metal_lead_in EQUAL 11025
   OR NOT metal_pre_score_wait EQUAL 11025
   OR NOT metal_hit_hold EQUAL 2425
   OR NOT metal_hit_gap EQUAL 1249
   OR NOT metal_phrase_gap EQUAL 15434
   OR NOT metal_tail EQUAL 35280
   OR NOT metal_hit_count EQUAL 40
   OR NOT metal_total_frames EQUAL 250592
   OR NOT metal_pick_style STREQUAL "alternate_relatched_per_phrase"
   OR NOT metal_pick_keyswitch EQUAL 14
   OR NOT metal_keyswitch_velocity EQUAL 1
   OR NOT metal_velocity_count EQUAL 2
   OR NOT metal_velocity_accent EQUAL 0.94999999
   OR NOT metal_velocity_secondary EQUAL 0.81999999
   OR NOT metal_phrase_count EQUAL 4
   OR NOT metal_pickup_selector STREQUAL "bridge"
   OR NOT metal_output_mode STREQUAL "mono"
   OR NOT metal_amp_model STREQUAL "modern_high_gain"
   OR NOT metal_distortion EQUAL 0.44999999
   OR NOT metal_amp EQUAL 0.94999999
   OR NOT metal_compressor EQUAL 0.60000002
   OR NOT metal_delay EQUAL 0
   OR NOT metal_room EQUAL 0
   OR NOT metal_output_count EQUAL 2)
    message(FATAL_ERROR "Metal benchmark manifest contract changed unexpectedly")
endif()

set(metal_expected_target_ids e1 e2)
set(metal_expected_target_strings 8 6)
set(metal_expected_target_notes 28 40)
foreach(metal_target_index RANGE 0 1)
    list(GET metal_expected_target_ids ${metal_target_index}
        metal_expected_target_id)
    list(GET metal_expected_target_strings ${metal_target_index}
        metal_expected_target_string)
    list(GET metal_expected_target_notes ${metal_target_index}
        metal_expected_target_note)
    string(JSON metal_target_id GET "${metal_manifest}"
        protocol targets ${metal_target_index} id)
    string(JSON metal_target_string GET "${metal_manifest}"
        protocol targets ${metal_target_index} string)
    string(JSON metal_target_fret GET "${metal_manifest}"
        protocol targets ${metal_target_index} fret)
    string(JSON metal_target_note GET "${metal_manifest}"
        protocol targets ${metal_target_index} midi_note)
    if(NOT metal_target_id STREQUAL metal_expected_target_id
       OR NOT metal_target_string EQUAL metal_expected_target_string
       OR NOT metal_target_fret EQUAL 0
       OR NOT metal_target_note EQUAL metal_expected_target_note)
        message(FATAL_ERROR
            "Metal benchmark target ${metal_target_index} changed unexpectedly")
    endif()
endforeach()

set(metal_expected_styles palm_mute dead palm_mute dead)
set(metal_expected_hits 12 12 8 8)
set(metal_expected_pattern_lengths 1 1 4 4)
set(metal_expected_style_keyswitches 16 21 16 21)
foreach(metal_phrase_index RANGE 0 3)
    list(GET metal_expected_styles ${metal_phrase_index} metal_expected_style)
    list(GET metal_expected_hits ${metal_phrase_index} metal_expected_phrase_hits)
    list(GET metal_expected_pattern_lengths ${metal_phrase_index}
        metal_expected_pattern_length)
    list(GET metal_expected_style_keyswitches ${metal_phrase_index}
        metal_expected_style_keyswitch)
    string(JSON metal_phrase_style GET "${metal_manifest}"
        protocol phrases ${metal_phrase_index} play_style)
    string(JSON metal_phrase_hits GET "${metal_manifest}"
        protocol phrases ${metal_phrase_index} hits)
    string(JSON metal_style_keyswitch GET "${metal_manifest}"
        protocol phrases ${metal_phrase_index} play_style_keyswitch_midi_note)
    string(JSON metal_pattern_length LENGTH "${metal_manifest}"
        protocol phrases ${metal_phrase_index} midi_note_pattern)
    math(EXPR metal_last_pattern_index "${metal_pattern_length} - 1")
    if(NOT metal_phrase_style STREQUAL metal_expected_style
       OR NOT metal_phrase_hits EQUAL metal_expected_phrase_hits
       OR NOT metal_style_keyswitch EQUAL metal_expected_style_keyswitch
       OR NOT metal_pattern_length EQUAL metal_expected_pattern_length
       )
        message(FATAL_ERROR
            "Metal benchmark phrase ${metal_phrase_index} changed unexpectedly")
    endif()
    foreach(metal_pattern_index RANGE 0 ${metal_last_pattern_index})
        string(JSON metal_pattern_note GET "${metal_manifest}"
            protocol phrases ${metal_phrase_index} midi_note_pattern
            ${metal_pattern_index})
        set(metal_expected_pattern_note 28)
        if(metal_pattern_length EQUAL 4 AND metal_pattern_index EQUAL 3)
            set(metal_expected_pattern_note 40)
        endif()
        if(NOT metal_pattern_note EQUAL metal_expected_pattern_note)
            message(FATAL_ERROR
                "Metal benchmark phrase ${metal_phrase_index} note pattern changed")
        endif()
    endforeach()
endforeach()

set(metal_expected_ids pre_fx_dry_di post_fx_high_gain)
set(metal_expected_files
    metal-e1-e2-pre-fx-dry-di.wav
    metal-e1-e2-post-fx-high-gain.wav)
file(GLOB metal_wavs "${metal_pass_a}/*.wav")
list(LENGTH metal_wavs metal_wav_count)
if(NOT metal_wav_count EQUAL 2)
    message(FATAL_ERROR
        "Metal benchmark directory has ${metal_wav_count} WAVs; expected 2")
endif()

foreach(metal_output_index RANGE 0 1)
    list(GET metal_expected_ids ${metal_output_index} metal_expected_id)
    list(GET metal_expected_files ${metal_output_index} metal_expected_file)
    string(JSON metal_output_id GET "${metal_manifest}"
        outputs ${metal_output_index} id)
    string(JSON metal_output_file GET "${metal_manifest}"
        outputs ${metal_output_index} file)
    string(JSON metal_output_frames GET "${metal_manifest}"
        outputs ${metal_output_index} frames)
    string(JSON metal_output_peak GET "${metal_manifest}"
        outputs ${metal_output_index} raw_peak)
    if(NOT metal_output_id STREQUAL metal_expected_id
       OR NOT metal_output_file STREQUAL metal_expected_file
       OR NOT metal_output_frames EQUAL 250592
       OR metal_output_peak LESS 0.0001)
        message(FATAL_ERROR
            "Metal benchmark output ${metal_output_index} failed its contract")
    endif()

    file(SIZE "${metal_pass_a}/${metal_output_file}" metal_wav_size)
    if(NOT metal_wav_size EQUAL 1002412)
        message(FATAL_ERROR
            "${metal_output_file} has ${metal_wav_size} bytes; expected 1002412")
    endif()
    file(SHA256 "${metal_pass_a}/${metal_output_file}" metal_hash_a)
    file(SHA256 "${metal_pass_b}/${metal_output_file}" metal_hash_b)
    if(NOT metal_hash_a STREQUAL metal_hash_b)
        message(FATAL_ERROR
            "${metal_output_file} is not byte-identical across deterministic renders")
    endif()
    list(APPEND metal_output_hashes "${metal_hash_a}")
endforeach()

list(GET metal_output_hashes 0 metal_dry_hash)
list(GET metal_output_hashes 1 metal_wet_hash)
if(metal_dry_hash STREQUAL metal_wet_hash)
    message(FATAL_ERROR "Metal benchmark dry and distorted WAVs are identical")
endif()
file(SHA256 "${metal_pass_a}/manifest.json" metal_manifest_hash_a)
file(SHA256 "${metal_pass_b}/manifest.json" metal_manifest_hash_b)
if(NOT metal_manifest_hash_a STREQUAL metal_manifest_hash_b)
    message(FATAL_ERROR
        "Metal benchmark manifests are not byte-identical across renders")
endif()

message(STATUS
    "${render_output}${metal_render_output}Evaluation output contracts passed")
