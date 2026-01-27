add_library(frontend
    frontend/ast.cpp
    frontend/lexer.hpp
    frontend/parser.hpp
    frontend/lexer.cpp
    frontend/parser.cpp
    frontend/symbol_table.cpp
    frontend/elab.cpp
)

target_include_directories(frontend PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_library(ir
    ir/rtl_ir.cpp
    ir/ir_builder.cpp
)

target_include_directories(ir PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ir PUBLIC frontend)

add_library(sim
    sim/value.cpp
    sim/process.cpp
    sim/vcd_writer.cpp
    sim/kernel.cpp
)

target_include_directories(sim PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(sim PUBLIC ir)

add_library(synth
    synth/netlist.cpp
    synth/techmap.cpp
    synth/synth_driver.cpp
)

target_include_directories(synth PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(synth PUBLIC ir)

add_library(dpi
    dpi/dpi_shim.cpp
)

target_include_directories(dpi PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(dpi PUBLIC sim ir)

add_library(uvm
    uvm/uvm_env.cpp
)

target_include_directories(uvm PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(uvm PUBLIC dpi sim ir)

add_library(coverage
    coverage/coverage.cpp
)

target_include_directories(coverage PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_library(sva
    sva/sva_engine.cpp
)

target_include_directories(sva PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(sva PUBLIC sim)

add_executable(svtool
    cli/main.cpp
)

target_link_libraries(svtool
    frontend
    ir
    sim
    synth
    coverage
    sva
)
