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
