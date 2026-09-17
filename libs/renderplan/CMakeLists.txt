add_library(saudade_renderplan
    src/render_plan.cpp
)

target_include_directories(saudade_renderplan
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/libs/audio/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(saudade_renderplan
    PUBLIC
        saudade_time
        saudade_events
)

saudade_set_warnings(saudade_renderplan)
