add_library(saudade_time
    src/tempo_map.cpp
    src/transport.cpp
)

target_include_directories(saudade_time
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

saudade_set_warnings(saudade_time)
