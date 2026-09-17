add_library(saudade_audio
    src/audio_buffer.cpp
    src/allocation_guard.cpp
    src/engine.cpp
    src/offline_endpoint.cpp
    src/plan_publisher.cpp
)

target_include_directories(saudade_audio
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(saudade_audio
    PUBLIC
        saudade_renderplan
        saudade_time
)

saudade_set_warnings(saudade_audio)
