add_library(saudade_graph
    src/graph_model.cpp
    src/graph_compiler.cpp
)

target_include_directories(saudade_graph
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(saudade_graph
    PUBLIC
        saudade_renderplan
)

saudade_set_warnings(saudade_graph)
