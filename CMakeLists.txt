cmake_minimum_required(VERSION 3.31)
project(poly_allocator VERSION 5.0 LANGUAGES CXX)

set (CMAKE_CXX_STANDARD 23)

#static since it's all templates anyway
add_library(poly-allocator STATIC
  arena_allocator.hpp
)
target_include_directories(poly-allocator PUBLIC .)
set_target_properties(poly-allocator PROPERTIES LINKER_LANGUAGE CXX)
