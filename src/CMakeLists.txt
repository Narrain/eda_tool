add_library(frontend
    frontend/ast.cpp
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
