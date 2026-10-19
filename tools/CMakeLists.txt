# Ручной инструмент для живой проверки на реальном приборе -- не часть ctest.
add_executable(live_smoke_test live_smoke_test.cpp)
target_link_libraries(live_smoke_test PRIVATE ${PROJECT_NAME})
add_executable(status_probe status_probe.cpp)
target_link_libraries(status_probe PRIVATE ${PROJECT_NAME})

# retomhold uses POSIX-only APIs directly (flock()/<sys/file.h> for its
# exclusive port lock, <unistd.h> for getpid()/close()) -- no Windows
# equivalent implemented, since this dev-only diagnostic has so far only
# ever been built and run on the Linux bench. This whole tools/ directory is
# already never built when libavm-retom is consumed as a dependency (see the
# top-level CMakeLists.txt's CMAKE_CURRENT_SOURCE_DIR STREQUAL
# CMAKE_SOURCE_DIR guard) -- that excludes it from every avm-tester build,
# Windows included, regardless of host OS. This guard covers the separate,
# real gap: building libavm-retom standalone (this same tools/ directory)
# directly on a Windows dev machine, which the dependency guard does not
# prevent.
if(NOT WIN32)
  add_executable(retomhold retomhold.cpp)
  target_link_libraries(retomhold PRIVATE ${PROJECT_NAME})
endif()
