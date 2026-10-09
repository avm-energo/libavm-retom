# Ручной инструмент для живой проверки на реальном приборе -- не часть ctest.
add_executable(live_smoke_test live_smoke_test.cpp)
target_link_libraries(live_smoke_test PRIVATE ${PROJECT_NAME})
add_executable(status_probe status_probe.cpp)
target_link_libraries(status_probe PRIVATE ${PROJECT_NAME})
add_executable(retomhold retomhold.cpp)
target_link_libraries(retomhold PRIVATE ${PROJECT_NAME})
