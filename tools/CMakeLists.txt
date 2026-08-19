# Ручной инструмент для живой проверки на реальном приборе -- не часть ctest.
add_executable(live_smoke_test live_smoke_test.cpp)
target_link_libraries(live_smoke_test PRIVATE ${PROJECT_NAME})
