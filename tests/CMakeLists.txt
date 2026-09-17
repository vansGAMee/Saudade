add_executable(test_offline_engine
    test_offline_engine.cpp
)
target_link_libraries(test_offline_engine
    PRIVATE
        saudade_graph
        saudade_audio
)
saudade_set_warnings(test_offline_engine)
add_test(NAME offline_engine COMMAND test_offline_engine)

add_executable(test_graph_compiler
    test_graph_compiler.cpp
)
target_link_libraries(test_graph_compiler
    PRIVATE
        saudade_graph
)
saudade_set_warnings(test_graph_compiler)
add_test(NAME graph_compiler COMMAND test_graph_compiler)

add_executable(test_allocation_guard
    test_allocation_guard.cpp
)
target_link_libraries(test_allocation_guard
    PRIVATE
        saudade_graph
        saudade_audio
)
saudade_set_warnings(test_allocation_guard)
add_test(NAME allocation_guard COMMAND test_allocation_guard)

add_executable(test_plan_publication
    test_plan_publication.cpp
)
target_link_libraries(test_plan_publication
    PRIVATE
        saudade_graph
        saudade_audio
)
saudade_set_warnings(test_plan_publication)
add_test(NAME plan_publication COMMAND test_plan_publication)

add_executable(test_quantum_consistency
    test_quantum_consistency.cpp
)
target_link_libraries(test_quantum_consistency
    PRIVATE
        saudade_graph
        saudade_audio
)
saudade_set_warnings(test_quantum_consistency)
add_test(NAME quantum_consistency COMMAND test_quantum_consistency)

add_executable(test_safe_reclamation
    test_safe_reclamation.cpp
)
target_link_libraries(test_safe_reclamation
    PRIVATE
        saudade_graph
        saudade_audio
)
saudade_set_warnings(test_safe_reclamation)
add_test(NAME safe_reclamation COMMAND test_safe_reclamation)

add_executable(test_stress_concurrency
    test_stress_concurrency.cpp
)
target_link_libraries(test_stress_concurrency
    PRIVATE
        saudade_graph
        saudade_audio
)
saudade_set_warnings(test_stress_concurrency)
add_test(NAME stress_concurrency COMMAND test_stress_concurrency)

add_executable(test_time_and_tempo
    test_time_and_tempo.cpp
)
target_link_libraries(test_time_and_tempo
    PRIVATE
        saudade_time
)
saudade_set_warnings(test_time_and_tempo)
add_test(NAME time_and_tempo COMMAND test_time_and_tempo)

add_executable(test_transport
    test_transport.cpp
)
target_link_libraries(test_transport
    PRIVATE
        saudade_graph
        saudade_audio
        saudade_time
)
saudade_set_warnings(test_transport)
add_test(NAME transport COMMAND test_transport)

add_executable(test_events
    test_events.cpp
)
target_link_libraries(test_events
    PRIVATE
        saudade_graph
        saudade_audio
        saudade_time
        saudade_events
)
saudade_set_warnings(test_events)
add_test(NAME events COMMAND test_events)

add_executable(test_poly_synth
    test_poly_synth.cpp
)
target_link_libraries(test_poly_synth
    PRIVATE
        saudade_graph
        saudade_audio
        saudade_time
        saudade_events
)
saudade_set_warnings(test_poly_synth)
add_test(NAME poly_synth COMMAND test_poly_synth)


