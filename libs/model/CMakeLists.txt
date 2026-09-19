add_library(saudade_model
    src/note_sequence.cpp
    src/pattern.cpp
    src/pattern_compiler.cpp
    src/project.cpp
    src/midi_import.cpp
)

target_include_directories(saudade_model
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(saudade_model
    PUBLIC
        saudade_time
        saudade_events
)

saudade_set_warnings(saudade_model)
