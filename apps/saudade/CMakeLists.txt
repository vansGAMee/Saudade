set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

add_library(saudade_ui STATIC
    include/saudade/ui/coordinates.hpp
    include/saudade/ui/editor_controller.hpp
    include/saudade/ui/piano_roll_item.hpp
    include/saudade/ui/piano_keys_item.hpp
    include/saudade/ui/project_file.hpp
    include/saudade/ui/project_exporter.hpp
    src/editor_controller.cpp
    src/piano_roll_item.cpp
    src/piano_keys_item.cpp
    src/project_file.cpp
    src/project_exporter.cpp
)

target_include_directories(saudade_ui
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(saudade_ui
    PUBLIC
        Qt6::Core
        Qt6::Gui
        Qt6::Quick
        Qt6::Qml
        saudade_graph
        saudade_audio
        saudade_time
        saudade_events
        saudade_model
)

saudade_set_warnings(saudade_ui)

add_executable(saudade
    main.cpp
    resources.qrc
)

target_compile_definitions(saudade
    PRIVATE
        SAUDADE_QML_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(saudade
    PRIVATE
        saudade_ui
        saudade_pipewire
)

saudade_set_warnings(saudade)
