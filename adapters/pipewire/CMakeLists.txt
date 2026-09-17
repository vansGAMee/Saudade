add_library(saudade_pipewire
    src/pipewire_endpoint.cpp
)

target_include_directories(saudade_pipewire
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(saudade_pipewire
    PUBLIC
        saudade_audio
        PkgConfig::PIPEWIRE
)

saudade_set_warnings(saudade_pipewire)
