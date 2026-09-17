add_executable(audio-proof
    main.cpp
)

target_link_libraries(audio-proof
    PRIVATE
        saudade_graph
        saudade_audio
        saudade_pipewire
        saudade_time
        saudade_events
        saudade_model
)

saudade_set_warnings(audio-proof)
