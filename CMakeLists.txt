cmake_minimum_required(VERSION 3.31)
project(poly_allocator VERSION 5.0 LANGUAGES CXX)

set (CMAKE_CXX_STANDARD 23)

cmake_minimum_required(VERSION 3.14)
project(poly-allocator)

set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

#static since it's all templates anyway
add_library(poly-allocator STATIC
  lib/arena_allocator.hpp
  lib/arena_allocator.cpp
)
target_include_directories(poly-allocator PUBLIC lib)
set_target_properties(poly-allocator PROPERTIES LINKER_LANGUAGE CXX)

add_executable(poly-allocator-test
  test/data.h test/data.cpp
  test/common.h test/common.cpp
  test/exception_handling.h test/exception_handling.cpp

  test/arena_allocator_test.cpp

  test/arena_allocator_test_deallocate.cpp
  test/arena_allocator_test_clear.cpp
)
target_include_directories(poly-allocator-test PUBLIC test)
target_link_libraries(poly-allocator-test poly-allocator GTest::gtest_main)
