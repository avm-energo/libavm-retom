# Примеры использования библиотеки -- НЕ входят в ctest, требуют реального прибора.
add_executable(connect_wait_disconnect connect_wait_disconnect.cpp)
target_link_libraries(connect_wait_disconnect PRIVATE ${PROJECT_NAME})
