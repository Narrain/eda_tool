add_executable(test_frontend_basic
    frontend/unit/test_frontend_basic.cpp
)
target_link_libraries(test_frontend_basic frontend)
add_test(NAME test_frontend_basic COMMAND test_frontend_basic)

add_executable(test_frontend_elab
    frontend/unit/test_frontend_elab.cpp
)
target_link_libraries(test_frontend_elab frontend)
add_test(NAME test_frontend_elab COMMAND test_frontend_elab)

add_executable(test_frontend_regression
    frontend/regression/test_frontend_regression.cpp
)
target_link_libraries(test_frontend_regression frontend)
add_test(NAME test_frontend_regression COMMAND test_frontend_regression)

add_executable(test_frontend_negative
    frontend/negative/test_frontend_negative.cpp
)
target_link_libraries(test_frontend_negative frontend)
add_test(NAME test_frontend_negative COMMAND test_frontend_negative)

add_executable(test_ir_basic
    ir/unit/test_ir_basic.cpp
)
target_link_libraries(test_ir_basic frontend ir)
add_test(NAME test_ir_basic COMMAND test_ir_basic)

add_executable(test_ir_builder
    ir/unit/test_ir_builder.cpp
)
target_link_libraries(test_ir_builder frontend ir)
add_test(NAME test_ir_builder COMMAND test_ir_builder)

add_executable(test_ir_regression
    ir/regression/test_ir_regression.cpp
)
target_link_libraries(test_ir_regression frontend ir)
add_test(NAME test_ir_regression COMMAND test_ir_regression)

add_executable(test_ir_negative
    ir/negative/test_ir_negative.cpp
)
target_link_libraries(test_ir_negative frontend ir)
add_test(NAME test_ir_negative COMMAND test_ir_negative)

add_executable(test_sim_value
    sim/unit/test_sim_value.cpp
)
target_link_libraries(test_sim_value sim)
add_test(NAME test_sim_value COMMAND test_sim_value)

add_executable(test_sim_kernel_basic
    sim/unit/test_sim_kernel_basic.cpp
)
target_link_libraries(test_sim_kernel_basic sim)
add_test(NAME test_sim_kernel_basic COMMAND test_sim_kernel_basic)

add_executable(test_sim_regression
    sim/regression/test_sim_regression.cpp
)
target_link_libraries(test_sim_regression sim)
add_test(NAME test_sim_regression COMMAND test_sim_regression)

add_executable(test_synth_basic
    synth/unit/test_synth_basic.cpp
)
target_link_libraries(test_synth_basic synth)
add_test(NAME test_synth_basic COMMAND test_synth_basic)

add_executable(test_synth_regression
    synth/regression/test_synth_regression.cpp
)
target_link_libraries(test_synth_regression synth)
add_test(NAME test_synth_regression COMMAND test_synth_regression)

add_executable(test_synth_negative
    synth/negative/test_synth_negative.cpp
)
target_link_libraries(test_synth_negative synth)
add_test(NAME test_synth_negative COMMAND test_synth_negative)

add_executable(test_dpi_basic
    dpi/unit/test_dpi_basic.cpp
)
target_link_libraries(test_dpi_basic dpi)
add_test(NAME test_dpi_basic COMMAND test_dpi_basic)

add_executable(test_uvm_smoke
    uvm/unit/test_uvm_smoke.cpp
)
target_link_libraries(test_uvm_smoke uvm)
add_test(NAME test_uvm_smoke COMMAND test_uvm_smoke)

add_executable(test_coverage_basic
    coverage/unit/test_coverage_basic.cpp
)
target_link_libraries(test_coverage_basic coverage)
add_test(NAME test_coverage_basic COMMAND test_coverage_basic)

add_executable(test_sva_basic
    sva/unit/test_sva_basic.cpp
)
target_link_libraries(test_sva_basic sva sim)
add_test(NAME test_sva_basic COMMAND test_sva_basic)
