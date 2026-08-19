find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Test)

function(retom_add_test name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE ${PROJECT_NAME} Qt${QT_VERSION_MAJOR}::Test)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

retom_add_test(test_framing)
retom_add_test(test_protocol)
retom_add_test(test_driver)
