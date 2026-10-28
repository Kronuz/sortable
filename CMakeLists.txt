cmake_minimum_required(VERSION 3.14)
project(sortable LANGUAGES CXX)

# Header-only, no dependencies: sortable.hh is the whole implementation.
add_library(sortable INTERFACE)
target_include_directories(sortable INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(sortable INTERFACE cxx_std_20)
add_library(sortable::sortable ALIAS sortable)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  enable_testing()
  find_package(Threads REQUIRED)

  # The format's examples, the call forms, the model's vectors, the reject fixture, the short inputs.
  add_executable(sortable_test test/test.cc)
  target_link_libraries(sortable_test PRIVATE sortable Threads::Threads)
  target_compile_definitions(sortable_test PRIVATE SORTABLE_TEST_DIR="${CMAKE_CURRENT_SOURCE_DIR}/test")
  add_test(NAME sortable COMMAND sortable_test)

  # Round trip, order and cross-type equality against an exact comparator.
  add_executable(sortable_property test/property.cc)
  target_link_libraries(sortable_property PRIVATE sortable)
  add_test(NAME sortable_property COMMAND sortable_property)

  add_executable(sortable_size test/size.cc)
  target_link_libraries(sortable_size PRIVATE sortable)
  add_test(NAME sortable_size COMMAND sortable_size)

  # The fuzz target's body runs anywhere through a replay driver; libFuzzer (clang) is opt-in.
  add_executable(sortable_standalone_fuzz fuzz/decode_fuzz.cc fuzz/standalone.cc)
  target_link_libraries(sortable_standalone_fuzz PRIVATE sortable)
  option(SORTABLE_FUZZ "Build the libFuzzer target" OFF)
  if(SORTABLE_FUZZ)
    add_executable(sortable_fuzz fuzz/decode_fuzz.cc)
    target_link_libraries(sortable_fuzz PRIVATE sortable)
    target_compile_options(sortable_fuzz PRIVATE -fsanitize=fuzzer,address,undefined -fno-sanitize-recover=all)
    target_link_options(sortable_fuzz PRIVATE -fsanitize=fuzzer,address,undefined)
  endif()

  # Every float, minutes long: not in CI.
  option(SORTABLE_EXHAUSTIVE "Build the exhaustive binary32 check" OFF)
  if(SORTABLE_EXHAUSTIVE)
    add_executable(sortable_exhaustive_float test/exhaustive_float.cc)
    target_link_libraries(sortable_exhaustive_float PRIVATE sortable)
  endif()

  # The fixtures are the model's output: rerun it and compare (skipped without Python 3).
  find_package(Python3 COMPONENTS Interpreter QUIET)
  if(Python3_FOUND)
    add_test(NAME sortable_fixtures
      COMMAND ${CMAKE_COMMAND}
        -DPYTHON=${Python3_EXECUTABLE} -DROOT=${CMAKE_CURRENT_SOURCE_DIR} -DWORK=${CMAKE_CURRENT_BINARY_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tools/check_fixtures.cmake)
  endif()
endif()
