add_library(saudade_events
    src/events.cpp
)

target_include_directories(saudade_events
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(saudade_events
    PUBLIC
        saudade_time
)

saudade_set_warnings(saudade_events)
