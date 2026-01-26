add_library(frontend
    frontend/ast.cpp
    frontend/lexer.cpp
    frontend/parser.cpp
    frontend/symbol_table.cpp
    frontend/elab.cpp
)

target_include_directories(frontend PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
