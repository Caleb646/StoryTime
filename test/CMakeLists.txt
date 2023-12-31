cmake_minimum_required(VERSION 3.21)
project(pst_reader_tests VERSION 0.1.0)

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(pst_reader_tests "test_main.cpp")
target_include_directories(pst_reader_tests PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(pst_reader_tests PRIVATE GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(pst_reader_tests)