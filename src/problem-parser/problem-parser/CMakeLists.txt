# CMakeList.txt : CMake project for problem-parser, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.22)

# Add source to this project's executable.
add_executable (problem-parser "problem-parser.cpp" "problem-parser.h")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET problem-parser PROPERTY CXX_STANDARD 20)
endif()

# TODO: Add tests and install targets if needed.
